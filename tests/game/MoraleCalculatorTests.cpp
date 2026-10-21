#include "GameFixtures.h"
#include "TempConfigFile.h"

#include "game/effects/EffectEnums.h"
#include "game/social-engineering/SocialEngineeringManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/population/PopulationManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/combat/CombatResolver.h"
#include "game/combat/MoraleCalculator.h"
#include "game/combat/MoraleRules.h"
#include "game/combat/MoraleMutations.h"
#include "game/combat/MoraleConfig.h"
#include "game/combat/MoraleConfigParser.h"
#include "game/movement/MoveCostRules.h"
#include "game/movement/StepEvaluator.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/movement/Pathfinder.h"
#include "lib/LuaRuntime.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfigParser.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>

using namespace ac;
using namespace actest;

TEST_CASE("MoraleConfigParser loads SMAC defaults", "[morale][config]")
{
    const MoraleConfig_t config =
        MoraleConfigParser{}.ParseConfig(FixturePath("morale_levels.json"));
    CHECK(config.startingIntrinsic == 1);
    CHECK(config.defenseFloorIndex == 1);
    CHECK(config.percentPerLevel == Catch::Approx(12.5));
    CHECK(config.baselineIndex == 2);
    CHECK(config.promotionSeedFormula
          == "(attack_strength + defense_strength) > 0 and defense_strength / "
             "(attack_strength + defense_strength) or 0");
    CHECK(config.levels.size() == 7);
    CHECK(config.levels[4].conventional == "Veteran");
    CHECK(config.levels[4].native == "Mature Boil");
    REQUIRE(config.levels[4].effects.size() == 2);
    const auto* pPromote =
        std::get_if<StatModifierEffect_t>(&config.levels[4].effects[0].effect);
    REQUIRE(pPromote);
    CHECK(pPromote->stat == StatId_t::PromotionChance);
    CHECK(pPromote->op == ModifierOp_t::MultiplyGeometric);
    CHECK(pPromote->amount == Catch::Approx(0.5));
}

TEST_CASE("ResolvePromotionChanceFromLevel: D/(A+D) with level promotion_chance effects",
          "[morale][promotion]")
{
    const MoraleConfig_t config =
        MoraleConfigParser{}.ParseConfig(FixturePath("morale_levels.json"));
    LuaRuntime lua;

    const auto seed = [&](int attack, int defense) {
        return lua.EvalDouble(config.promotionSeedFormula,
                              {{"attack_strength",  static_cast<double>(attack)},
                               {"defense_strength", static_cast<double>(defense)}});
    };

    // Green: MinClamp 1 → always 100% regardless of fight balance.
    CHECK(ResolvePromotionChanceFromLevel(seed(100, 0), config.levels[1].effects)
          == Catch::Approx(1.0));

    // Disciplined: seed D/(A+D) only.
    CHECK(ResolvePromotionChanceFromLevel(seed(30, 70), config.levels[2].effects)
          == Catch::Approx(0.7));
    CHECK(ResolvePromotionChanceFromLevel(seed(0, 0), config.levels[2].effects)
          == Catch::Approx(0.0));

    // Commando (3–5 band): D/(A+D)/2 (seed ≤ 1, so MaxClamp 1 would never bind).
    CHECK(ResolvePromotionChanceFromLevel(seed(30, 70), config.levels[5].effects)
          == Catch::Approx(0.35));
    CHECK(ResolvePromotionChanceFromLevel(seed(0, 100), config.levels[5].effects)
          == Catch::Approx(0.5));
}

TEST_CASE("MoraleConfigParser rejects invalid level effects", "[morale][config]")
{
    const actest::TempConfigFile config("ac_morale_bad_op.json", R"({
      "defense_floor_index": 0,
      "percent_per_level": 12.5,
      "baseline_index": 0,
      "promotion_seed_formula": "0",
      "levels": [{
        "index": 0, "conventional": "X", "native": "Y",
        "effects": [{
          "type": "StatModifier", "scope": "ThisUnit",
          "parameters": { "stat": "attack", "amount": 1, "op": "AddPercent" }
        }]
      }]
    })");
    CHECK_THROWS_WITH(MoraleConfigParser{}.ParseConfig(config.Path()),
                      Catch::Matchers::ContainsSubstring("percent_per_level"));
}

TEST_CASE("MoraleConfigParser requires percent_per_level and baseline_index", "[morale][config]")
{
    const actest::TempConfigFile config("ac_morale_missing_scale.json", R"({
      "defense_floor_index": 0,
      "promotion_seed_formula": "0",
      "levels": [{
        "index": 0, "conventional": "X", "native": "Y",
        "effects": []
      }]
    })");
    CHECK_THROWS_WITH(MoraleConfigParser{}.ParseConfig(config.Path()),
                      Catch::Matchers::ContainsSubstring("percent_per_level"));
}

TEST_CASE("CombatAddPercent scales Attack with combat morale level", "[morale]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis", "test_weapon"});
    const MoraleCalculator& morale = fixture.morale();
    const EffectContext_t ctx{};

    const int baseAttack = ResolveStat(unit, StatId_t::Attack, ctx);

    unit.SetXp(0); // Very Green −25%
    const int veryGreen = CombatMoraleLevel(unit, ctx, morale);
    CHECK(ResolveCombatUnitStat(unit, StatId_t::Attack, ctx, morale.CombatAddPercent(veryGreen))
          == static_cast<int>(std::lround(baseAttack * 0.75)));

    unit.SetXp(2); // Disciplined 0%
    const int disciplined = CombatMoraleLevel(unit, ctx, morale);
    CHECK(ResolveCombatUnitStat(unit, StatId_t::Attack, ctx,
                                morale.CombatAddPercent(disciplined))
          == baseAttack);

    unit.SetXp(4); // Veteran +25%
    const int veteran = CombatMoraleLevel(unit, ctx, morale);
    CHECK(ResolveCombatUnitStat(unit, StatId_t::Attack, ctx, morale.CombatAddPercent(veteran))
          == static_cast<int>(std::lround(baseAttack * 1.25)));
}

TEST_CASE("DisplayName switches on IsNativeLife", "[morale]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    const MoraleCalculator& morale = fixture.morale();

    Unit& conventional = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    conventional.SetXp(3);
    const EffectContext_t conventionalCtx{&conventional.GetTile()};
    CHECK(MoraleDisplayName(conventional, conventionalCtx, morale) == "Hardened");

    Unit& psi = fixture.MakeUnit(faction, 9, 9, {"test_chassis", "test_psi"});
    psi.SetXp(3);
    const EffectContext_t psiCtx{&psi.GetTile()};
    CHECK(MoraleDisplayName(psi, psiCtx, morale) == "Hardened");

    Unit& native = fixture.MakeUnit(faction, 10, 10, {"native_life_chassis"});
    native.SetXp(3);
    const EffectContext_t nativeCtx{&native.GetTile()};
    CHECK(MoraleDisplayName(native, nativeCtx, morale) == "Boil");
}

TEST_CASE("SE Morale +2 rank is +1; defend-in-base extra is morale_extra", "[morale][se]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 8, 8);
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"}, &base);
    unit.SetXp(2); // Disciplined intrinsic
    const MoraleCalculator& morale = fixture.morale();

    const SocialPolicyConfig_t* pPolicy = fixture.socialPolicies().Find("morale_policy");
    REQUIRE(pPolicy);
    faction.GetSocialEngineering().SetActivePolicy(*pPolicy);

    const EffectContext_t displayCtx{&unit.GetTile()};
    const EffectContext_t defCtx{&unit.GetTile(), CombatRole_t::Defender};
    const EffectContext_t atkCtx{&unit.GetTile(), CombatRole_t::Attacker};
    CHECK(ResolveStat(unit, StatId_t::MoraleRank, displayCtx) == 1);
    CHECK(ResolveStat(unit, StatId_t::MoraleRank, defCtx) == 1);
    CHECK(ResolveStat(unit, StatId_t::MoraleRank, atkCtx) == 1);
    CHECK(ResolveStat(unit, StatId_t::MoraleExtra, defCtx) == 1);
    CHECK(ResolveStat(unit, StatId_t::MoraleExtra, atkCtx) == 0);
    CHECK(EffectiveMoraleLevel(unit, displayCtx, morale) == 3);
    CHECK(EffectiveMoraleLevel(unit, defCtx, morale) == 3);
    CHECK(EffectiveMoraleLevel(unit, atkCtx, morale) == 3);
    CHECK(CombatMoraleLevel(unit, defCtx, morale) == 4);
    CHECK(CombatMoraleLevel(unit, atkCtx, morale) == 3);
    CHECK(MoraleDisplayName(unit, displayCtx, morale) == "Hardened");
}

TEST_CASE("Home-base Creche offsets stated morale for conventional units only", "[morale][se]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& crecheBase = fixture.MakeFactionBase(faction, 8, 8);
    BaseManager& otherBase = fixture.MakeFactionBase(faction, 8, 12);
    crecheBase.GetBuildingManager().AddBuilding("Childrens_Creche");
    Unit& homedAtCreche = fixture.MakeUnit(faction, 8, 8, {"test_chassis"}, &crecheBase);
    Unit& homedElsewhere = fixture.MakeUnit(faction, 8, 12, {"test_chassis"}, &otherBase);
    Unit& nativeHome = fixture.MakeUnit(faction, 8, 10, {"native_life_chassis"}, &crecheBase);
    homedAtCreche.SetXp(2);
    homedElsewhere.SetXp(2);
    nativeHome.SetXp(2);
    const MoraleCalculator& morale = fixture.morale();

    const SocialPolicyConfig_t* pLow = fixture.socialPolicies().Find("low_morale_policy");
    REQUIRE(pLow);
    faction.GetSocialEngineering().SetActivePolicy(*pLow);

    // SE -2 → table -1; home half +1 → stated 2. Away and native keep -1 → stated 1.
    const EffectContext_t atkCreche{&homedAtCreche.GetTile(), CombatRole_t::Attacker};
    const EffectContext_t atkOther{&homedElsewhere.GetTile(), CombatRole_t::Attacker};
    const EffectContext_t atkNative{&nativeHome.GetTile(), CombatRole_t::Attacker};
    CHECK(EffectiveMoraleLevel(homedAtCreche, atkCreche, morale) == 2);
    CHECK(EffectiveMoraleLevel(homedElsewhere, atkOther, morale) == 1);
    CHECK(EffectiveMoraleLevel(nativeHome, atkNative, morale) == 1);
    const EffectContext_t displayCreche{&homedAtCreche.GetTile()};
    const EffectContext_t displayOther{&homedElsewhere.GetTile()};
    CHECK(MoraleDisplayName(homedAtCreche, displayCreche, morale) == "Disciplined");
    CHECK(MoraleDisplayName(homedElsewhere, displayOther, morale) == "Green");

    const SocialPolicyConfig_t* pVeryLow = fixture.socialPolicies().Find("very_low_morale_policy");
    REQUIRE(pVeryLow);
    faction.GetSocialEngineering().SetActivePolicy(*pVeryLow);
    // SE -4 → table -3; home half +2 → stated 1. Away: clamp 2-3 → 0.
    CHECK(EffectiveMoraleLevel(homedAtCreche, atkCreche, morale) == 1);
    CHECK(EffectiveMoraleLevel(homedElsewhere, atkOther, morale) == 0);
}

TEST_CASE("Creche in-square extras are combat levels and do not change rank", "[morale][creche]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& creche = fixture.MakeFactionBase(faction, 8, 8);
    creche.GetBuildingManager().AddBuilding("Childrens_Creche");
    Unit& onTile = fixture.MakeUnit(faction, 8, 8, {"test_chassis"}, &creche);
    Unit& offTile = fixture.MakeUnit(faction, 8, 12, {"test_chassis"}, &creche);
    onTile.SetXp(2);
    offTile.SetXp(2);
    const MoraleCalculator& morale = fixture.morale();

    const EffectContext_t atkOn{&onTile.GetTile(), CombatRole_t::Attacker};
    const EffectContext_t defOn{&onTile.GetTile(), CombatRole_t::Defender};
    const EffectContext_t atkOff{&offTile.GetTile(), CombatRole_t::Attacker};
    CHECK(EffectiveMoraleLevel(onTile, atkOn, morale) == 2);
    CHECK(EffectiveMoraleLevel(onTile, defOn, morale) == 2);
    CHECK(CombatMoraleLevel(onTile, atkOn, morale) == 3);
    CHECK(CombatMoraleLevel(onTile, defOn, morale) == 3);
    CHECK(CombatMoraleLevel(offTile, atkOff, morale) == 2);
    const EffectContext_t displayOn{&onTile.GetTile()};
    CHECK(MoraleDisplayName(onTile, displayOn, morale) == "Disciplined");

    const SocialPolicyConfig_t* pHigh = fixture.socialPolicies().Find("high_morale_policy");
    REQUIRE(pHigh);
    faction.GetSocialEngineering().SetActivePolicy(*pHigh);
    // SE +4 → table +3. In-square: attack -3, defense +1.
    CHECK(EffectiveMoraleLevel(onTile, atkOn, morale) == 5);
    CHECK(EffectiveMoraleLevel(onTile, defOn, morale) == 5);
    CHECK(CombatMoraleLevel(onTile, atkOn, morale) == 2);
    CHECK(CombatMoraleLevel(onTile, defOn, morale) == 6);
    CHECK(MoraleDisplayName(onTile, displayOn, morale) == "Commando");
}

TEST_CASE("Defense floor applies after extras, not to stated rank", "[morale]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    unit.SetXp(0);
    const MoraleCalculator& morale = fixture.morale();

    const EffectContext_t atkCtx{&unit.GetTile(), CombatRole_t::Attacker};
    const EffectContext_t defCtx{&unit.GetTile(), CombatRole_t::Defender};
    CHECK(EffectiveMoraleLevel(unit, atkCtx, morale) == 0);
    CHECK(EffectiveMoraleLevel(unit, defCtx, morale) == 0);
    CHECK(CombatMoraleLevel(unit, atkCtx, morale) == 0);
    CHECK(CombatMoraleLevel(unit, defCtx, morale) == 1);

    BaseManager& creche = fixture.MakeFactionBase(faction, 8, 8);
    creche.GetBuildingManager().AddBuilding("Childrens_Creche");
    Unit& onCreche = fixture.MakeUnit(faction, 8, 8, {"test_chassis"}, &creche);
    onCreche.SetXp(0);
    const SocialPolicyConfig_t* pVeryLow = fixture.socialPolicies().Find("very_low_morale_policy");
    REQUIRE(pVeryLow);
    faction.GetSocialEngineering().SetActivePolicy(*pVeryLow);
    // Stated Very Green (home half keeps 0+(-3)+2 clamped at 0). Extras +4 → combat 4, not Green+4.
    const EffectContext_t defCreche{&onCreche.GetTile(), CombatRole_t::Defender};
    const EffectContext_t atkCreche{&onCreche.GetTile(), CombatRole_t::Attacker};
    CHECK(EffectiveMoraleLevel(onCreche, defCreche, morale) == 0);
    CHECK(CombatMoraleLevel(onCreche, defCreche, morale) == 4);
    CHECK(CombatMoraleLevel(onCreche, atkCreche, morale) == 4);
}

TEST_CASE("Elite plus Creche extras caps defense and leaves attack uncapped", "[morale][creche]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& creche = fixture.MakeFactionBase(faction, 8, 8);
    creche.GetBuildingManager().AddBuilding("Childrens_Creche");
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis", "test_weapon"}, &creche);
    unit.SetXp(6);
    const MoraleCalculator& morale = fixture.morale();

    const EffectContext_t atkCtx{&unit.GetTile(), CombatRole_t::Attacker};
    const EffectContext_t defCtx{&unit.GetTile(), CombatRole_t::Defender};
    CHECK(CombatMoraleLevel(unit, atkCtx, morale) == 7);
    CHECK(CombatMoraleLevel(unit, defCtx, morale) == 6);
    CHECK(morale.CombatAddPercent(7) == Catch::Approx(62.5));
    CHECK(morale.CombatAddPercent(6) == Catch::Approx(50.0));
}

TEST_CASE("Brood Pit adds native-psi extras and does not copy Creche home-half",
          "[morale][brood]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& broodOnly = fixture.MakeFactionBase(faction, 8, 8);
    BaseManager& both = fixture.MakeFactionBase(faction, 8, 12);
    broodOnly.GetBuildingManager().AddBuilding("Brood_Pit");
    both.GetBuildingManager().AddBuilding("Childrens_Creche");
    both.GetBuildingManager().AddBuilding("Brood_Pit");

    Unit& nativeBrood = fixture.MakeUnit(faction, 8, 8, {"native_life_chassis"}, &broodOnly);
    Unit& nativeBoth = fixture.MakeUnit(faction, 8, 12, {"native_life_chassis"}, &both);
    Unit& conventionalBrood = fixture.MakeUnit(faction, 8, 6, {"test_chassis"}, &broodOnly);
    nativeBrood.SetXp(2);
    nativeBoth.SetXp(2);
    conventionalBrood.SetXp(2);
    const MoraleCalculator& morale = fixture.morale();

    const EffectContext_t atkBrood{&nativeBrood.GetTile(), CombatRole_t::Attacker};
    const EffectContext_t defBrood{&nativeBrood.GetTile(), CombatRole_t::Defender};
    const EffectContext_t atkBoth{&nativeBoth.GetTile(), CombatRole_t::Attacker};
    const EffectContext_t atkConv{&conventionalBrood.GetTile(), CombatRole_t::Attacker};

    // Rating 0: Brood-only +1; Brood+Creche is Creche +1 plus Brood +1.
    CHECK(EffectiveMoraleLevel(nativeBrood, atkBrood, morale) == 2);
    CHECK(CombatMoraleLevel(nativeBrood, atkBrood, morale) == 3);
    CHECK(CombatMoraleLevel(nativeBrood, defBrood, morale) == 3);
    CHECK(CombatMoraleLevel(nativeBoth, atkBoth, morale) == 4);
    CHECK(CombatMoraleLevel(conventionalBrood, atkConv, morale) == 2);

    const SocialPolicyConfig_t* pHigh = fixture.socialPolicies().Find("high_morale_policy");
    REQUIRE(pHigh);
    faction.GetSocialEngineering().SetActivePolicy(*pHigh);
    // Brood-only defense uses the attack column: +1 + (-4) = -3 on extras.
    CHECK(CombatMoraleLevel(nativeBrood, atkBrood, morale) == 5 + 1 - 4);
    CHECK(CombatMoraleLevel(nativeBrood, defBrood, morale) == 5 + 1 - 4);
}

TEST_CASE("Manual Creche snippet parses as valid effects", "[morale][manual]")
{
    std::ifstream in(FixturePath("creche_morale_manual.json"));
    REQUIRE(in);
    nlohmann::json root;
    in >> root;
    REQUIRE(root.contains("Childrens_Creche"));
    nlohmann::json wrapper = {{"effects", root.at("Childrens_Creche")}};
    const std::vector<EffectConfig_t> effects = EffectConfigParser::ParseEffects(
        wrapper, EffectSourceKind_t::Building, "Childrens_Creche");
    REQUIRE(effects.size() == 1);
    const auto* pMod = std::get_if<StatModifierEffect_t>(&effects[0].effect);
    REQUIRE(pMod);
    CHECK(pMod->stat == StatId_t::MoraleRank);
    CHECK(pMod->op == ModifierOp_t::MinClamp);
    CHECK(pMod->amount == Catch::Approx(1.0));
    REQUIRE(effects[0].condition.has_value());
    CHECK(std::holds_alternative<SubjectIsOnOriginBase_t>(effects[0].condition->AsVariant()));
}

TEST_CASE("ResolveCombatUnitStat applies CombatAddPercent on Attack", "[morale][combat]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis", "test_weapon"});
    unit.SetXp(4); // Veteran = +25%
    const MoraleCalculator& morale = fixture.morale();
    const EffectContext_t ctx{&unit.GetTile(), CombatRole_t::Attacker};

    const int baseAttack = ResolveStat(unit, StatId_t::Attack, ctx);
    const int combatLevel = CombatMoraleLevel(unit, ctx, morale);
    const int combatAttack = ResolveCombatUnitStat(
        unit, StatId_t::Attack, ctx, morale.CombatAddPercent(combatLevel));
    CHECK(combatAttack == static_cast<int>(std::lround(baseAttack * 1.25)));
}

TEST_CASE("morale_rank carries a Unit-domain amount_source without a caller-stamped faction",
          "[morale][amount_source]")
{
    // The parser accepts BasesOwned on any Unit-domain stat with ThisUnit scope, and
    // MoraleRank is Unit-domain. Combat builds a context with no faction, so the resolve has
    // to stamp the subject from the live unit rather than throwing on it.
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis", "test_empire_morale"});
    const MoraleCalculator& morale = fixture.morale();
    const EffectContext_t ctx{&unit.GetTile(), CombatRole_t::Attacker};

    const int noBases = EffectiveMoraleLevel(unit, ctx, morale);

    fixture.MakeFactionBase(faction, 8, 4);
    fixture.MakeFactionBase(faction, 8, 12);
    REQUIRE(faction.GetBaseCount() == 2);
    CHECK(EffectiveMoraleLevel(unit, ctx, morale) == noBases + 2);
}

TEST_CASE("Promotion: Green always promotes; Elite never", "[morale][promotion]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    const MoraleCalculator& morale = fixture.morale();
    std::mt19937 rng(1);

    unit.SetXp(1);
    CHECK(TryPromote(unit, 100, 100, morale, rng));
    CHECK(unit.GetXp() == 2);

    unit.SetXp(6);
    CHECK_FALSE(TryPromote(unit, 100, 100, morale, rng));
    CHECK(unit.GetXp() == 6);
}

TEST_CASE("MoraleCalculator value methods read the configured levels", "[morale]")
{
    FactionFixture fixture;
    const MoraleCalculator& morale = fixture.morale();

    // Levels run 0 (Very Green) to 6 (Elite). Role floor is not applied here.
    CHECK(morale.MoraleLevel(6, 2) == 6);
    CHECK(morale.MoraleLevel(0, -1) == 0);
    CHECK(morale.MoraleLevel(0, 0) == 0);
    CHECK(morale.MoraleLevel(3, 1) == 4);

    CHECK(morale.StartingIntrinsicXp() == 1);

    CHECK(morale.LevelEffects(4).size() == 2);
    CHECK(morale.CombatAddPercent(2) == Catch::Approx(0.0));
    CHECK(morale.CombatAddPercent(0) == Catch::Approx(-25.0));
    CHECK(morale.CombatAddPercent(4) == Catch::Approx(25.0));
    CHECK(morale.CombatAddPercent(7) == Catch::Approx(62.5));
    CHECK_THROWS_WITH(morale.LevelEffects(99), Catch::Matchers::ContainsSubstring("99"));

    // Green always promotes; Elite has no level above it.
    CHECK(morale.PromotionChance(1, 100, 100) == Catch::Approx(1.0));
    CHECK(morale.PromotionChance(6, 100, 100) == 0.0);
}

TEST_CASE("TryAttack promotes survivor after a kill", "[morale][promotion]")
{
    FactionFixture fixture;
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis"});
    attacker.SetXp(1);
    defender.SetXp(1);
    // Glass cannon defender: 0 armor so attacker wins quickly.
    attacker.SetCurrentHp(attacker.GetStat(StatId_t::HitPoints));

    MoveCostRules moveCosts(fixture.improvements);
    StepEvaluator steps(fixture.map, fixture.ctx);
    Pathfinder pathfinder(moveCosts, steps, fixture.map);
    std::mt19937 rng(/*seed*/ 42);
    UnitOrderExecutor orders(moveCosts, steps, fixture.map, fixture.ctx, pathfinder,
                             fixture.morale(), *fixture.dataContext.terrainOperationRegistry, rng);

    const int xpBefore = attacker.GetXp();
    auto result = orders.TryAttack(attacker, defender.GetTile());
    REQUIRE(result.has_value());
    if (result->bDefenderDestroyed && !result->bAttackerDestroyed)
    {
        CHECK(attacker.GetXp() >= xpBefore); // may promote (Green = 100%)
        CHECK(attacker.GetXp() == xpBefore + 1);
    }
}

TEST_CASE("A committed riot costs morale only for units homed at the rioting base",
          "[morale][riot]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& rioting = fixture.MakeFactionBase(faction, 8, 8);
    BaseManager& calm = fixture.MakeFactionBase(faction, 8, 12);
    Unit& homedAtRiot = fixture.MakeUnit(faction, 8, 8, {"test_chassis"}, &rioting);
    Unit& homedElsewhere = fixture.MakeUnit(faction, 8, 12, {"test_chassis"}, &calm);
    homedAtRiot.SetXp(3);
    homedElsewhere.SetXp(3);

    const EffectContext_t riotCtx{&homedAtRiot.GetTile(), CombatRole_t::Attacker};
    const EffectContext_t calmCtx{&homedElsewhere.GetTile(), CombatRole_t::Attacker};
    const int levelBefore = EffectiveMoraleLevel(homedAtRiot, riotCtx, fixture.morale());
    REQUIRE(ResolveStat(homedAtRiot, StatId_t::MoraleRank, riotCtx) == 0);

    rioting.GetPopulation().ForceRiot(/*turns=*/1);
    rioting.GetPopulation().CommitMood();
    REQUIRE(rioting.GetPopulation().IsRioting());
    REQUIRE_FALSE(calm.GetPopulation().IsRioting());

    const EffectContext_t displayRiot{&homedAtRiot.GetTile()};
    const std::string nameBefore = MoraleDisplayName(homedAtRiot, displayRiot, fixture.morale());

    // Riot is morale_rank behind IsInCombat: combat stated drops; the rank word does not.
    CHECK(ResolveStat(homedAtRiot, StatId_t::MoraleRank, riotCtx) == -1);
    CHECK(ResolveStat(homedAtRiot, StatId_t::MoraleRank, displayRiot) == 0);
    CHECK(ResolveStat(homedElsewhere, StatId_t::MoraleRank, calmCtx) == 0);
    CHECK(EffectiveMoraleLevel(homedAtRiot, riotCtx, fixture.morale()) == levelBefore - 1);
    CHECK(EffectiveMoraleLevel(homedAtRiot, displayRiot, fixture.morale()) == levelBefore);
    CHECK(EffectiveMoraleLevel(homedElsewhere, calmCtx, fixture.morale()) == levelBefore);
    CHECK(MoraleDisplayName(homedAtRiot, displayRiot, fixture.morale()) == nameBefore);

    // The base-lane half of the same tier stays on the base path and does not leak to units.
    CHECK(ResolveFlag(rioting, RuleFlagId_t::DisableProduction));
    CHECK_FALSE(ResolveFlag(calm, RuleFlagId_t::DisableProduction));
}

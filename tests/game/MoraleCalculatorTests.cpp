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

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <random>

using namespace ac;
using namespace actest;

TEST_CASE("MoraleConfigParser loads SMAC defaults", "[morale][config]")
{
    const MoraleConfig_t config =
        MoraleConfigParser{}.ParseConfig(FixturePath("morale_levels.json"));
    CHECK(config.baseIntrinsic == 1);
    CHECK(config.probeBaseIntrinsic == 2);
    CHECK(config.defenseFloorIndex == 1);
    CHECK(config.promotionSeedFormula
          == "(attack_strength + defense_strength) > 0 and defense_strength / "
             "(attack_strength + defense_strength) or 0");
    CHECK(config.levels.size() == 7);
    CHECK(config.levels[4].conventional == "Veteran");
    CHECK(config.levels[4].native == "Mature Boil");
    REQUIRE(config.levels[4].effects.size() == 4);
    const auto* pAtk = std::get_if<StatModifierEffect_t>(&config.levels[4].effects[0].effect);
    REQUIRE(pAtk);
    CHECK(pAtk->stat == StatId_t::Attack);
    CHECK(pAtk->amount == Catch::Approx(25.0));
    CHECK(pAtk->op == ModifierOp_t::AddPercent);
    const auto* pPromote =
        std::get_if<StatModifierEffect_t>(&config.levels[4].effects[2].effect);
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
      "promotion_seed_formula": "0",
      "levels": [{
        "index": 0, "conventional": "X", "native": "Y",
        "effects": [{
          "type": "StatModifier", "scope": "ThisUnit",
          "parameters": { "stat": "attack", "amount": 1, "op": "Add" }
        }]
      }]
    })");
    CHECK_THROWS_WITH(MoraleConfigParser{}.ParseConfig(config.Path()),
                      Catch::Matchers::ContainsSubstring("AddPercent"));
}

TEST_CASE("Morale level effects apply Attack AddPercent in combat resolve", "[morale]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis", "test_weapon"});
    const MoraleCalculator& morale = fixture.morale();
    const EffectContext_t ctx{};

    const int baseAttack = ResolveStat(unit, StatId_t::Attack, ctx);

    unit.SetXp(0); // Very Green −25%
    CHECK(ResolveCombatUnitStat(unit, StatId_t::Attack, ctx,
                                EffectiveLevelEffects(unit, ctx, morale))
          == static_cast<int>(std::lround(baseAttack * 0.75)));

    unit.SetXp(2); // Disciplined 0%
    CHECK(ResolveCombatUnitStat(unit, StatId_t::Attack, ctx,
                                EffectiveLevelEffects(unit, ctx, morale))
          == baseAttack);

    unit.SetXp(4); // Veteran +25%
    CHECK(ResolveCombatUnitStat(unit, StatId_t::Attack, ctx,
                                EffectiveLevelEffects(unit, ctx, morale))
          == static_cast<int>(std::lround(baseAttack * 1.25)));
}

TEST_CASE("DisplayName switches on IsNativeLife", "[morale]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    const MoraleCalculator& morale = fixture.morale();

    Unit& conventional = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    conventional.SetXp(3);
    CHECK(MoraleDisplayName(conventional, morale) == "Hardened");

    Unit& psi = fixture.MakeUnit(faction, 9, 9, {"test_chassis", "test_psi"});
    psi.SetXp(3);
    CHECK(MoraleDisplayName(psi, morale) == "Hardened");

    Unit& native = fixture.MakeUnit(faction, 10, 10, {"native_life_chassis"});
    native.SetXp(3);
    CHECK(MoraleDisplayName(native, morale) == "Boil");
}

TEST_CASE("SE Morale +2 adds defense-in-base morale_bonus only when defending", "[morale][se]")
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

    const EffectContext_t defCtx{&unit.GetTile(), CombatRole_t::Defender};
    const EffectContext_t atkCtx{&unit.GetTile(), CombatRole_t::Attacker};
    // +2 SE: +1 always, +1 more defending in base → 2 on defense, 1 on attack.
    CHECK(ResolveStat(unit, StatId_t::MoraleBonus, defCtx) == 2);
    CHECK(ResolveStat(unit, StatId_t::MoraleBonus, atkCtx) == 1);
    CHECK(EffectiveMoraleLevel(unit, defCtx, morale) == 4);
    CHECK(EffectiveMoraleLevel(unit, atkCtx, morale) == 3);
}

TEST_CASE("Low SE Morale halves positive Creche defense bonus", "[morale][se]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 8, 8);
    base.GetBuildingManager().AddBuilding("Childrens_Creche");
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"}, &base);
    unit.SetXp(2);
    const MoraleCalculator& morale = fixture.morale();

    const SocialPolicyConfig_t* pPolicy = fixture.socialPolicies().Find("low_morale_policy");
    REQUIRE(pPolicy);
    faction.GetSocialEngineering().SetActivePolicy(*pPolicy);

    const EffectContext_t defCtx{&unit.GetTile(), CombatRole_t::Defender};
    // SE -2: morale_bonus -1 + positive_morale_scale -50%; Creche +1 → 0 after scale.
    // Creche home soften: unconditional -1 → 0; conditional scaled → 0 → level 2.
    CHECK(EffectiveMoraleLevel(unit, defCtx, morale) == 2);
}

TEST_CASE("Defense floor keeps defender at Green", "[morale]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis"});
    unit.SetXp(0); // Very Green
    const MoraleCalculator& morale = fixture.morale();

    const EffectContext_t atkCtx{&unit.GetTile(), CombatRole_t::Attacker};
    const EffectContext_t defCtx{&unit.GetTile(), CombatRole_t::Defender};
    CHECK(EffectiveMoraleLevel(unit, atkCtx, morale) == 0);
    CHECK(EffectiveMoraleLevel(unit, defCtx, morale) == 1);
}

TEST_CASE("ResolveCombatUnitStat applies morale level Attack AddPercent", "[morale][combat]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 8, 8, {"test_chassis", "test_weapon"});
    unit.SetXp(4); // Veteran = +25%
    const MoraleCalculator& morale = fixture.morale();
    const EffectContext_t ctx{&unit.GetTile(), CombatRole_t::Attacker};

    const int baseAttack = ResolveStat(unit, StatId_t::Attack, ctx);
    const int combatAttack = ResolveCombatUnitStat(
        unit, StatId_t::Attack, ctx, EffectiveLevelEffects(unit, ctx, morale));
    CHECK(combatAttack == static_cast<int>(std::lround(baseAttack * 1.25)));
}

TEST_CASE("morale_bonus carries a Unit-domain amount_source without a caller-stamped faction",
          "[morale][amount_source]")
{
    // The parser accepts BasesOwned on any Unit-domain stat with ThisUnit scope, and
    // MoraleBonus is Unit-domain. Combat builds a context with no faction, so the resolve has
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

    // Levels run 0 (Very Green) to 6 (Elite); the defense floor is 1 (Green).
    CHECK(morale.MoraleLevel(6, 2, CombatRole_t::Attacker) == 6);
    CHECK(morale.MoraleLevel(0, -1, CombatRole_t::Attacker) == 0);
    CHECK(morale.MoraleLevel(0, 0, CombatRole_t::Defender) == 1);
    CHECK(morale.MoraleLevel(3, 1, CombatRole_t::Defender) == 4);

    CHECK(morale.IntrinsicXp(false) == 1);
    CHECK(morale.IntrinsicXp(true) == 2);

    CHECK(morale.LevelEffects(4).size() == 4);
    CHECK(morale.LevelEffects(99).empty());

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
    REQUIRE(ResolveStat(homedAtRiot, StatId_t::MoraleBonus, riotCtx) == 0);

    rioting.GetPopulation().ForceRiot(/*turns=*/1);
    rioting.GetPopulation().CommitMood();
    REQUIRE(rioting.GetPopulation().IsRioting());
    REQUIRE_FALSE(calm.GetPopulation().IsRioting());

    // The riot tier declares this penalty FactionUnits-scoped with an OriginBaseIsHomeBase
    // condition, so it can only work if the tier's faction-lane effects reach the faction
    // pool: a FactionUnits effect sitting in the rioting base's own effect list is read by
    // nothing. It must also narrow to that base's own garrison.
    CHECK(ResolveStat(homedAtRiot, StatId_t::MoraleBonus, riotCtx) == -1);
    CHECK(ResolveStat(homedElsewhere, StatId_t::MoraleBonus, calmCtx) == 0);
    CHECK(EffectiveMoraleLevel(homedAtRiot, riotCtx, fixture.morale()) == levelBefore - 1);
    CHECK(EffectiveMoraleLevel(homedElsewhere, calmCtx, fixture.morale()) == levelBefore);

    // The base-lane half of the same tier stays on the base path and does not leak to units.
    CHECK(ResolveFlag(rioting, RuleFlagId_t::DisableProduction));
    CHECK_FALSE(ResolveFlag(calm, RuleFlagId_t::DisableProduction));
}

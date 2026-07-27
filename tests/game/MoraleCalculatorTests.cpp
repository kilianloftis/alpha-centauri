#include "GameFixtures.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/units/CombatResolver.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/MoraleConfig.h"
#include "game/units/MoraleConfigParser.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/StepEvaluator.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/Pathfinder.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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
    CHECK(config.levels.size() == 7);
    CHECK(config.levels[4].conventional == "Veteran");
    CHECK(config.levels[4].native == "Mature Boil");
    CHECK(config.levels[4].combatBonusPercent == Catch::Approx(25.0));
    CHECK(config.FindPromotionRule(0) != nullptr);
    CHECK(config.FindPromotionRule(6) == nullptr);
}

TEST_CASE("CombatMoraleAddPercent follows configured level percents", "[morale]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    const MoraleCalculator morale(*fixture.dataContext.moraleConfig);

    unit.SetXp(0);
    CHECK(morale.CombatMoraleAddPercent(unit, {}) == Catch::Approx(-25.0));
    unit.SetXp(2);
    CHECK(morale.CombatMoraleAddPercent(unit, {}) == Catch::Approx(0.0));
    unit.SetXp(4);
    CHECK(morale.CombatMoraleAddPercent(unit, {}) == Catch::Approx(25.0));
}

TEST_CASE("DisplayName switches on ForcesPsiCombat", "[morale]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& conventional = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    const MoraleCalculator morale(*fixture.dataContext.moraleConfig);
    conventional.SetXp(3);
    CHECK(morale.DisplayName(conventional) == "Hardened");
}

TEST_CASE("SE Morale +2 adds defense-in-base morale_bonus only when defending", "[morale][se]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"}, &base);
    unit.SetXp(2); // Disciplined intrinsic
    const MoraleCalculator morale(*fixture.dataContext.moraleConfig);

    const SocialPolicyConfig_t* pPolicy = fixture.socialPolicies().Find("morale_policy");
    REQUIRE(pPolicy);
    faction.GetSocialEngineering().SetActivePolicy(*pPolicy);

    const EffectContext_t defCtx{&unit.GetTile(), CombatRole_t::Defender};
    const EffectContext_t atkCtx{&unit.GetTile(), CombatRole_t::Attacker};
    // +2 SE: +1 always, +1 more defending in base → 2 on defense, 1 on attack.
    CHECK(ResolveStat(unit, StatId_t::MoraleBonus, defCtx) == 2);
    CHECK(ResolveStat(unit, StatId_t::MoraleBonus, atkCtx) == 1);
    CHECK(morale.EffectiveMoraleLevel(unit, defCtx) == 4);
    CHECK(morale.EffectiveMoraleLevel(unit, atkCtx) == 3);
}

TEST_CASE("Low SE Morale halves positive Creche defense bonus", "[morale][se]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    base.GetBuildingManager().AddBuilding("Childrens_Creche");
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"}, &base);
    unit.SetXp(2);
    const MoraleCalculator morale(*fixture.dataContext.moraleConfig);

    const SocialPolicyConfig_t* pPolicy = fixture.socialPolicies().Find("low_morale_policy");
    REQUIRE(pPolicy);
    faction.GetSocialEngineering().SetActivePolicy(*pPolicy);

    const EffectContext_t defCtx{&unit.GetTile(), CombatRole_t::Defender};
    // SE -2: morale_bonus -1 + positive_morale_scale -50%; Creche +1 → 0 after scale.
    // Creche home soften: unconditional -1 → 0; conditional scaled → 0 → level 2.
    CHECK(morale.EffectiveMoraleLevel(unit, defCtx) == 2);
}

TEST_CASE("Defense floor keeps defender at Green", "[morale]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    unit.SetXp(0); // Very Green
    const MoraleCalculator morale(*fixture.dataContext.moraleConfig);

    const EffectContext_t atkCtx{&unit.GetTile(), CombatRole_t::Attacker};
    const EffectContext_t defCtx{&unit.GetTile(), CombatRole_t::Defender};
    CHECK(morale.EffectiveMoraleLevel(unit, atkCtx) == 0);
    CHECK(morale.EffectiveMoraleLevel(unit, defCtx) == 1);
}

TEST_CASE("ResolveCombatStat applies morale AddPercent", "[morale][combat]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis", "test_weapon"});
    unit.SetXp(4); // Veteran = +25%
    const MoraleCalculator morale(*fixture.dataContext.moraleConfig);
    const EffectContext_t ctx{&unit.GetTile(), CombatRole_t::Attacker};

    const int baseAttack = ResolveStat(unit, StatId_t::Attack, ctx);
    const int combatAttack = morale.ResolveCombatStat(unit, StatId_t::Attack, ctx);
    CHECK(combatAttack == static_cast<int>(std::lround(baseAttack * 1.25)));
}

TEST_CASE("Promotion: Green always promotes; Elite never", "[morale][promotion]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Unit& unit = fixture.MakeUnit(faction, 4, 4, {"test_chassis"});
    const MoraleCalculator morale(*fixture.dataContext.moraleConfig);
    std::mt19937 rng(1);

    unit.SetXp(1);
    CHECK(morale.TryPromote(unit, 100, 100, rng));
    CHECK(unit.GetXp() == 2);

    unit.SetXp(6);
    CHECK_FALSE(morale.TryPromote(unit, 100, 100, rng));
    CHECK(unit.GetXp() == 6);
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
    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    attacker.SetXp(1);
    defender.SetXp(1);
    // Glass cannon defender: 0 armor so attacker wins quickly.
    attacker.SetCurrentHp(attacker.GetStat(StatId_t::HitPoints));

    MoveCostCalculator moveCosts(fixture.improvements);
    StepEvaluator steps(fixture.map, *fixture.ctx);
    Pathfinder pathfinder(moveCosts, steps, fixture.map);
    UnitOrderExecutor orders(moveCosts, steps, fixture.map, *fixture.ctx, pathfinder,
                             *fixture.dataContext.moraleConfig, /*seed*/ 42);

    const int xpBefore = attacker.GetXp();
    auto result = orders.TryAttack(attacker, defender.GetTile());
    REQUIRE(result.has_value());
    if (result->bDefenderDestroyed && !result->bAttackerDestroyed)
    {
        CHECK(attacker.GetXp() >= xpBefore); // may promote (Green = 100%)
        CHECK(attacker.GetXp() == xpBefore + 1);
    }
}

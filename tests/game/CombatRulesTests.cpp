#include "GameFixtures.h"

#include "game/combat/CombatResolver.h"
#include "game/combat/CombatRules.h"
#include "game/combat/MoraleRules.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/Faction.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace ac;
using namespace actest;

namespace
{

void FillLand_(WorldFixture& fixture)
{
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }
}

} // namespace

TEST_CASE("A tied round goes to the defender and only the attacker loses HP", "[combat]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis"});

    const CombatRoundOutcome_t tie =
        ResolveCombatRound(attacker, defender, 5, 5, CombatEngagement_t::Standard, 0);
    CHECK(tie.winner == CombatSide_t::Defender);
    CHECK(tie.attackerHpLoss == k_CombatRoundDamage);
    CHECK(tie.defenderHpLoss == 0);

    const CombatRoundOutcome_t win =
        ResolveCombatRound(attacker, defender, 6, 5, CombatEngagement_t::Standard, 0);
    CHECK(win.winner == CombatSide_t::Attacker);
    CHECK(win.attackerHpLoss == 0);
    CHECK(win.defenderHpLoss == k_CombatRoundDamage);
    CHECK(attacker.GetCurrentHp() == 10);
    CHECK(defender.GetCurrentHp() == 10);
}

TEST_CASE("A strike spares a losing attacker and stops the defender at the floor",
          "[combat][bombard]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis"});
    const int floor = 4;

    const CombatRoundOutcome_t miss =
        ResolveCombatRound(attacker, defender, 0, 5, CombatEngagement_t::ArtilleryStrike, floor);
    CHECK(miss.winner == CombatSide_t::Defender);
    CHECK(miss.damage == k_CombatRoundDamage);
    CHECK(miss.attackerHpLoss == 0);

    auto hitAt = [&](int defenderHp)
    {
        defender.SetCurrentHp(defenderHp);
        return ResolveCombatRound(attacker, defender, 5, 0, CombatEngagement_t::ArtilleryStrike,
                                  floor)
            .defenderHpLoss;
    };
    CHECK(hitAt(10) == k_CombatRoundDamage);
    CHECK(hitAt(floor) == 0);
    CHECK(hitAt(floor - 2) == 0);
}

TEST_CASE("A unit may disengage once it has lost half its starting HP", "[combat][disengage]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Unit& unit = fixture.MakeUnit(fixture.MakeFaction(), 8, 8, {"test_chassis"});

    unit.SetCurrentHp(6);
    CHECK_FALSE(PastDisengageThreshold(unit, 10));
    unit.SetCurrentHp(5);
    CHECK(PastDisengageThreshold(unit, 10));
}

TEST_CASE("Planning collateral marks base occupants immune and changes no unit",
          "[combat][collateral]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker =
        fixture.MakeUnit(player, 8, 8, {"test_chassis", "test_weapon", "test_collateral_2"});
    Unit& defender = fixture.MakeUnit(enemy, 9, 9, {"test_chassis"});
    Unit& inOpen = fixture.MakeUnit(enemy, 9, 9, {"test_chassis"});

    const std::vector<CollateralHit_t> open = PlanStackCollateral(attacker, defender, fixture.map);
    REQUIRE(open.size() == 1);
    CHECK(open.front().pOccupant == &inOpen);
    CHECK_FALSE(open.front().bImmune);
    CHECK(open.front().damage == 2);
    CHECK(inOpen.GetCurrentHp() == 10);

    fixture.ctx->AddOccupantWithEffects(fixture.At(9, 9), "Base");
    const std::vector<CollateralHit_t> inBase =
        PlanStackCollateral(attacker, defender, fixture.map);
    REQUIRE(inBase.size() == 1);
    CHECK(inBase.front().bImmune);
    CHECK(inBase.front().damage == 0);
}

TEST_CASE("Only the survivor of a kill is offered promotion", "[combat][morale]")
{
    CombatResult_t result;
    CHECK_FALSE(PromotionCandidate(result).has_value());

    result.bDefenderDestroyed = true;
    CHECK(PromotionCandidate(result) == CombatSide_t::Attacker);

    result.bAttackerDestroyed = true;
    CHECK_FALSE(PromotionCandidate(result).has_value());

    result.bDefenderDestroyed = false;
    CHECK(PromotionCandidate(result) == CombatSide_t::Defender);
}

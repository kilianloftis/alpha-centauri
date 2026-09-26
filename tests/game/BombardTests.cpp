#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/WorldRulesConfigParser.h"
#include "game/faction/FactionRevealedUnits.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/AttackRules.h"
#include "game/units/CombatResolver.h"
#include "game/units/CombatStrength.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/MovementConstants.h"
#include "game/units/Pathfinder.h"
#include "game/units/StepEvaluator.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

struct CombatHarness_
{
    MoveCostCalculator moveCosts;
    StepEvaluator steps;
    std::mt19937 rng;
    CombatResolver combat;

    CombatHarness_(FactionFixture& fixture, uint32_t seed)
        : moveCosts(fixture.improvements)
        , steps(fixture.map, *fixture.ctx)
        , rng(seed)
        , combat(moveCosts, steps, fixture.map, *fixture.ctx, fixture.morale(), rng)
    {
    }
};

struct OrderHarness_
{
    MoveCostCalculator moveCosts;
    StepEvaluator steps;
    Pathfinder pathfinder;
    std::mt19937 rng;
    UnitOrderExecutor orders;

    OrderHarness_(FactionFixture& fixture, uint32_t seed)
        : moveCosts(fixture.improvements)
        , steps(fixture.map, *fixture.ctx)
        , pathfinder(moveCosts, steps, fixture.map)
        , rng(seed)
        , orders(moveCosts, steps, fixture.map, *fixture.ctx, pathfinder, fixture.morale(), rng)
    {
    }
};

void FillLand_(WorldFixture& fixture)
{
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(100);
    }
}

std::vector<EffectConfig_t> LoadWorldRules_()
{
    return WorldRulesConfigParser{}.ParseConfig(FixturePath("world_rules.json"));
}

void BindFloors_(FactionFixture& fixture)
{
    fixture.ctx->BindWorldEffects(*fixture.pBindState);
}

int ResolvedFloorPercent_(const FactionFixture& fixture, const Tile& rTile)
{
    const std::vector<ActiveEffect_t> effects = fixture.ctx->CollectAreaEffects(rTile);
    return FinalizeResolvedStat(ResolveStatModifiers(
        FilterByStatId(effects, StatId_t::BombardMinHpPercent), 0.0).total);
}

CombatResolveOptions_t StrikeOptions_()
{
    CombatResolveOptions_t options;
    options.engagement = CombatEngagement_t::ArtilleryStrike;
    return options;
}

CombatResolveOptions_t DuelOptions_()
{
    CombatResolveOptions_t options;
    options.engagement = CombatEngagement_t::ArtilleryDuel;
    return options;
}

const CombatResult_t* FirstWinningStrike_(FactionFixture& fixture, Unit& rAttacker, Unit& rDefender,
                                          int startingHp)
{
    static CombatResult_t found;
    for (uint32_t seed = 1; seed <= 16; ++seed)
    {
        rDefender.SetCurrentHp(startingHp);
        rAttacker.SetCurrentHp(10);
        CombatHarness_ harness(fixture, seed);
        found = harness.combat.Resolve(rAttacker, rDefender, StrikeOptions_());
        if (!found.rounds.empty() && found.rounds.front().roundWinner == CombatSide_t::Attacker)
        {
            return &found;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("Bombard range includes adjacent and the configured edge", "[bombard]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "bombard"});

    CHECK(IsWithinBombardRange(attacker, fixture.At(5, 4), fixture.map));
    CHECK(IsWithinBombardRange(attacker, fixture.At(6, 4), fixture.map));
    CHECK_FALSE(IsWithinBombardRange(attacker, fixture.At(4, 4), fixture.map));
    CHECK_FALSE(IsWithinBombardRange(attacker, fixture.At(7, 4), fixture.map));

    OrderHarness_ orders(fixture, 1);
    const int before = attacker.GetMoveFragmentsRemaining();
    CHECK_FALSE(orders.orders.TryBombard(attacker, fixture.At(4, 4)));
    CHECK_FALSE(orders.orders.TryBombard(attacker, fixture.At(7, 4)));
    CHECK(attacker.GetMoveFragmentsRemaining() == before);

    Unit& plain = fixture.MakeUnit(player, 3, 3, {"test_chassis"});
    CHECK_FALSE(IsWithinBombardRange(plain, fixture.At(4, 3), fixture.map));
}

TEST_CASE("An artillery strike leaves the attacker unhurt and respects the HP floor", "[bombard]")
{
    FactionFixture fixture(9, 9, LoadWorldRules_());
    FillLand_(fixture);
    BindFloors_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "bombard"});
    Unit& defender = fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    attacker.SetXp(2);
    defender.SetXp(2);
    const int attackerHp = attacker.GetCurrentHp();

    CombatHarness_ harness(fixture, 1);
    const CombatResult_t lost = harness.combat.Resolve(attacker, defender, StrikeOptions_());
    REQUIRE(lost.rounds.size() == 1);
    CHECK(lost.rounds.front().roundWinner == CombatSide_t::Defender);
    CHECK(attacker.GetCurrentHp() == attackerHp);
    CHECK(defender.GetCurrentHp() == 10);

    CHECK(ResolvedFloorPercent_(fixture, defender.GetTile()) == 1);
    CHECK(static_cast<int>(std::ceil(10 * 1 / 100.0)) == 1);

    Unit& open = fixture.MakeUnit(enemy, 5, 5, {"test_chassis", "test_armor"});
    Unit& shooter = fixture.MakeUnit(player, 4, 5, {"test_chassis", "test_weapon", "bombard"});
    shooter.SetXp(2);
    open.SetXp(2);
    const CombatResult_t* pOpenHit = FirstWinningStrike_(fixture, shooter, open, 1);
    REQUIRE(pOpenHit != nullptr);
    CHECK(pOpenHit->rounds.size() == 1);
    CHECK(open.GetCurrentHp() == 1);

    open.SetCurrentHp(2);
    const CombatResult_t* pOpenDrop = FirstWinningStrike_(fixture, shooter, open, 2);
    REQUIRE(pOpenDrop != nullptr);
    CHECK(open.GetCurrentHp() == 1);

    fixture.At(6, 4).AddImprovement(fixture.improvements.Get("Bunker"));
    Unit& bunkered = fixture.MakeUnit(enemy, 6, 4, {"test_chassis"});
    Unit& bunkerShooter = fixture.MakeUnit(player, 6, 3, {"test_chassis", "test_weapon", "bombard"});
    bunkerShooter.SetXp(2);
    bunkered.SetXp(2);
    CHECK(ResolvedFloorPercent_(fixture, bunkered.GetTile()) == 50);
    CHECK(static_cast<int>(std::ceil(10 * 50 / 100.0)) == 5);
    CHECK(static_cast<int>(std::ceil(1 * 50 / 100.0)) == 1);

    const CombatResult_t* pAtFloor = FirstWinningStrike_(fixture, bunkerShooter, bunkered, 5);
    REQUIRE(pAtFloor != nullptr);
    CHECK(pAtFloor->rounds.size() == 1);
    CHECK(bunkered.GetCurrentHp() == 5);

    const CombatResult_t* pBelow = FirstWinningStrike_(fixture, bunkerShooter, bunkered, 2);
    REQUIRE(pBelow != nullptr);
    CHECK(bunkered.GetCurrentHp() == 2);

    const CombatResult_t* pOne = FirstWinningStrike_(fixture, bunkerShooter, bunkered, 1);
    REQUIRE(pOne != nullptr);
    CHECK(bunkered.GetCurrentHp() == 1);

    fixture.At(7, 4).AddImprovement(fixture.improvements.Get("Base"));
    CHECK(ResolvedFloorPercent_(fixture, fixture.At(7, 4)) == 50);
}

TEST_CASE("A strike rolls every hostile and cancels each order", "[bombard]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "bombard"});
    Unit& first = fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    Unit& second = fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    first.SetOrder(HoldOrder_t{});
    second.SetOrder(HoldOrder_t{});

    OrderHarness_ orders(fixture, 1);
    const auto result = orders.orders.TryBombard(attacker, fixture.At(5, 4));
    REQUIRE(result);
    REQUIRE(result->combats.size() == 2);
    CHECK(result->combats[0].rounds.size() == 1);
    CHECK(result->combats[1].rounds.size() == 1);
    CHECK(result->combats[0].bBombardPlayback);
    CHECK_FALSE(first.GetOrder().has_value());
    CHECK_FALSE(second.GetOrder().has_value());
    CHECK(attacker.GetCurrentHp() == 10);
    CHECK(attacker.GetMoveFragmentsRemaining() == 0);
}

TEST_CASE("An artillery duel rates the defender with attack and can destroy either side", "[bombard]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon", "bombard"});
    Unit& defender = fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "test_weapon", "test_armor", "bombard"});
    attacker.SetXp(2);
    defender.SetXp(2);
    defender.SetMoveFragmentsRemaining(1);

    const CombatStrength_t expected = ResolveCombatStrength(
        attacker, defender, *fixture.ctx, fixture.morale(), StatId_t::Attack);
    const EffectContext_t defenseCtx{&defender.GetTile(), CombatRole_t::Defender};
    EffectContext_t defenderAttack = defenseCtx;
    defenderAttack.pAttacker = &attacker;
    defenderAttack.pDefender = &defender;
    const int defenderAttackRating = ResolveCombatUnitStat(
        defender, StatId_t::Attack, defenderAttack,
        fixture.morale().EffectiveLevelEffects(defender, defenderAttack));
    const int point = MovementConstants_t::k_moveFragmentsPerPoint;
    const int scaledByLeftover = static_cast<int>(std::lround(
        defenderAttackRating * 1.0 / static_cast<double>(point) * k_CombatStrengthScale));

    CombatHarness_ harness(fixture, 1);
    const CombatResult_t duel = harness.combat.Resolve(attacker, defender, DuelOptions_());
    CHECK(duel.defenseStrength == expected.defenseStrength);
    CHECK(duel.defenseStrength == defenderAttackRating * k_CombatStrengthScale);
    CHECK(duel.defenseStrength != scaledByLeftover);
    CHECK_FALSE(duel.bAttackerDisengaged);
    CHECK_FALSE(duel.bDefenderDisengaged);

    Unit& fragileAttacker = fixture.MakeUnit(player, 3, 3, {"test_chassis", "bombard"});
    Unit& gunner = fixture.MakeUnit(enemy, 3, 4, {"test_chassis", "test_weapon", "bombard"});
    fragileAttacker.SetCurrentHp(10);
    CombatHarness_ hurt(fixture, 2);
    const CombatResult_t attackerFalls = hurt.combat.Resolve(fragileAttacker, gunner, DuelOptions_());
    CHECK(attackerFalls.bAttackerDestroyed);
    CHECK_FALSE(attackerFalls.bAttackerDisengaged);

    Unit& killer = fixture.MakeUnit(player, 2, 2, {"test_chassis", "test_weapon", "bombard"});
    Unit& fragileDefender = fixture.MakeUnit(enemy, 2, 3, {"test_slow_chassis", "bombard"});
    killer.SetXp(2);
    fragileDefender.SetXp(2);
    fragileDefender.SetCurrentHp(1);
    CombatHarness_ kill(fixture, 1);
    const CombatResult_t defenderDies = kill.combat.Resolve(killer, fragileDefender, DuelOptions_());
    CHECK(defenderDies.bDefenderDestroyed);
    CHECK_FALSE(defenderDies.bAttackerDisengaged);
    CHECK_FALSE(defenderDies.bDefenderDisengaged);

    Unit& fast = fixture.MakeUnit(player, 1, 1,
                                  {"test_chassis", "test_weapon", "bombard", "test_always_disengages"});
    Unit& slow = fixture.MakeUnit(enemy, 1, 2, {"test_slow_chassis", "test_weapon", "bombard"});
    fast.SetCurrentHp(2);
    CombatHarness_ noRetreat(fixture, 3);
    const CombatResult_t stayed = noRetreat.combat.Resolve(fast, slow, DuelOptions_());
    CHECK(stayed.bAttackerDestroyed);
    CHECK_FALSE(stayed.bAttackerDisengaged);
    CHECK(stayed.pRetreatTile == nullptr);
}

TEST_CASE("Partial movement scales bombard attack and the shot spends the rest", "[bombard]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon", "bombard"});
    Unit& defender = fixture.MakeUnit(enemy, 6, 4, {"test_chassis"});
    attacker.SetXp(2);
    defender.SetXp(2);
    const int point = MovementConstants_t::k_moveFragmentsPerPoint;
    attacker.SetMoveFragmentsRemaining(point * 2 / 3);

    const CombatStrength_t expectedStrike =
        ResolveCombatStrength(attacker, defender, *fixture.ctx, fixture.morale(),
                              StatId_t::Defense);
    OrderHarness_ orders(fixture, 1);
    const auto strike = orders.orders.TryBombard(attacker, defender.GetTile());
    REQUIRE(strike);
    REQUIRE(strike->combats.size() == 1);
    CHECK(strike->combats.front().attackStrength == expectedStrike.attackStrength);
    CHECK(attacker.GetMoveFragmentsRemaining() == 0);

    Unit& duelAttacker = fixture.MakeUnit(player, 4, 6, {"test_chassis", "test_weapon", "bombard"});
    Unit& duelDefender = fixture.MakeUnit(enemy, 6, 6, {"test_chassis", "test_weapon", "bombard"});
    duelAttacker.SetXp(2);
    duelDefender.SetXp(2);
    duelAttacker.SetMoveFragmentsRemaining(point * 2 / 3);
    duelDefender.SetMoveFragmentsRemaining(1);
    const CombatStrength_t expectedDuel = ResolveCombatStrength(
        duelAttacker, duelDefender, *fixture.ctx, fixture.morale(), StatId_t::Attack);
    const auto duel = orders.orders.TryBombard(duelAttacker, duelDefender.GetTile());
    REQUIRE(duel);
    REQUIRE(duel->combats.size() == 1);
    CHECK(duel->combats.front().attackStrength == expectedDuel.attackStrength);
    CHECK(duel->combats.front().defenseStrength == expectedDuel.defenseStrength);
    CHECK(duelAttacker.GetMoveFragmentsRemaining() == 0);
}

TEST_CASE("LevelsAboveOpponent adds 25 percent attack per whole level above the other unit",
          "[bombard]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon", "bombard"});
    Unit& defender = fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "test_weapon"});
    attacker.SetXp(2);
    defender.SetXp(2);

    fixture.At(4, 4).SetElevation(2000);
    fixture.At(5, 4).SetElevation(0);
    EffectContext_t ctx{&defender.GetTile(), CombatRole_t::Attacker};
    ctx.pAttacker = &attacker;
    ctx.pDefender = &defender;
    CHECK(ResolveCombatUnitStat(attacker, StatId_t::Attack, ctx,
                                fixture.morale().EffectiveLevelEffects(attacker, ctx))
          == 6);
    CHECK(attacker.GetStat(StatId_t::Attack) == 4);

    fixture.At(4, 4).SetElevation(999);
    CHECK(ResolveCombatUnitStat(attacker, StatId_t::Attack, ctx,
                                fixture.morale().EffectiveLevelEffects(attacker, ctx))
          == 4);

    fixture.At(4, 4).SetElevation(0);
    fixture.At(5, 4).SetElevation(2000);
    CHECK(ResolveCombatUnitStat(attacker, StatId_t::Attack, ctx,
                                fixture.morale().EffectiveLevelEffects(attacker, ctx))
          == 4);
}

TEST_CASE("An empty tile loses one non-base improvement, including outside vision", "[bombard]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "bombard"});
    player.RebuildVisibility();

    Tile& rFar = fixture.At(6, 4);
    rFar.AddImprovement(fixture.improvements.Get("Farm"));
    rFar.AddImprovement(fixture.improvements.Get("Base"));
    CHECK_FALSE(player.GetVisibleMap().IsVisible(rFar));

    OrderHarness_ orders(fixture, 1);
    const auto empty = orders.orders.TryBombard(attacker, rFar);
    REQUIRE(empty);
    CHECK(empty->combats.empty());
    CHECK(empty->destroyedImprovementId == "Farm");
    CHECK_FALSE(rFar.HasImprovement(ImprovementIds::k_Farm));
    CHECK(rFar.HasImprovement(ImprovementIds::k_Base));
    CHECK(attacker.GetMoveFragmentsRemaining() == 0);

    Unit& second = fixture.MakeUnit(player, 4, 2, {"test_chassis", "bombard"});
    Faction& enemy = fixture.MakeFaction();
    Unit& occupant = fixture.MakeUnit(enemy, 5, 2, {"test_chassis"});
    fixture.At(5, 2).AddImprovement(fixture.improvements.Get("Farm"));
    const auto occupied = orders.orders.TryBombard(second, fixture.At(5, 2));
    REQUIRE(occupied);
    CHECK_FALSE(occupied->destroyedImprovementId.has_value());
    CHECK(fixture.At(5, 2).HasImprovement(ImprovementIds::k_Farm));
    CHECK(occupant.GetCurrentHp() == 10);
}

TEST_CASE("Bombard hits fogged and concealed units and leaves them hidden", "[bombard]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();
    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "bombard"});
    Unit& fogged = fixture.MakeUnit(enemy, 6, 4, {"test_chassis"});
    fogged.SetOrder(HoldOrder_t{});
    player.RebuildVisibility();
    CHECK_FALSE(player.GetVisibleMap().IsVisible(fogged.GetTile()));
    CHECK_FALSE(player.GetRevealedUnits().IsRevealed(fogged));
    CHECK_FALSE(IsUnitVisibleTo(player, fogged, *fixture.ctx));

    OrderHarness_ orders(fixture, 1);
    const auto strike = orders.orders.TryBombard(attacker, fogged.GetTile());
    REQUIRE(strike);
    REQUIRE(strike->combats.size() == 1);
    CHECK_FALSE(player.GetRevealedUnits().IsRevealed(fogged));
    CHECK_FALSE(IsUnitVisibleTo(player, fogged, *fixture.ctx));
    CHECK_FALSE(fogged.GetOrder().has_value());
    CHECK_FALSE(player.GetVisibleMap().IsVisible(fogged.GetTile()));

    Unit& artillery = fixture.MakeUnit(enemy, 6, 6, {"test_chassis", "bombard", "Cloaking_Device"});
    Unit& bystander = fixture.MakeUnit(enemy, 6, 6, {"test_chassis"});
    artillery.SetOrder(HoldOrder_t{});
    bystander.SetOrder(HoldOrder_t{});
    player.RebuildVisibility();
    CHECK_FALSE(player.GetRevealedUnits().IsRevealed(artillery));
    CHECK_FALSE(IsUnitVisibleTo(player, artillery, *fixture.ctx));

    Unit& second = fixture.MakeUnit(player, 4, 6, {"test_chassis", "bombard"});
    const auto duel = orders.orders.TryBombard(second, artillery.GetTile());
    REQUIRE(duel);
    REQUIRE(duel->combats.size() == 1);
    CHECK(duel->combats.front().defenderId == artillery.GetUnitId());
    CHECK_FALSE(player.GetRevealedUnits().IsRevealed(artillery));
    CHECK_FALSE(player.GetRevealedUnits().IsRevealed(bystander));
    CHECK_FALSE(IsUnitVisibleTo(player, artillery, *fixture.ctx));
    CHECK_FALSE(IsUnitVisibleTo(player, bystander, *fixture.ctx));
    CHECK_FALSE(artillery.GetOrder().has_value());
    CHECK(bystander.GetOrder().has_value());
}

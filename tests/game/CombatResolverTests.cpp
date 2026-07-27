#include "GameFixtures.h"

#include "game/units/CombatResolver.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/MoraleConfig.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/Pathfinder.h"
#include "game/units/StepEvaluator.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/Faction.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace ac;
using namespace actest;

namespace
{

struct CombatHarness_
{
    MoveCostCalculator moveCosts;
    StepEvaluator steps;
    CombatResolver combat;

    CombatHarness_(FactionFixture& fixture, uint32_t seed)
        : moveCosts(fixture.improvements)
        , steps(fixture.map, *fixture.ctx)
        , combat(moveCosts, steps, fixture.map, *fixture.ctx, *fixture.dataContext.moraleConfig,
                 seed)
    {
    }
};

struct OrderHarness_
{
    MoveCostCalculator moveCosts;
    StepEvaluator steps;
    Pathfinder pathfinder;
    UnitOrderExecutor orders;

    OrderHarness_(FactionFixture& fixture, uint32_t seed)
        : moveCosts(fixture.improvements)
        , steps(fixture.map, *fixture.ctx)
        , pathfinder(moveCosts, steps, fixture.map)
        , orders(moveCosts, steps, fixture.map, *fixture.ctx, pathfinder,
                 *fixture.dataContext.moraleConfig, seed)
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

void FillWater_(WorldFixture& fixture)
{
    for (auto& pTile : fixture.map.GetTiles())
    {
        pTile->SetElevation(-100);
    }
}

} // namespace

TEST_CASE("Combat strength is resolved rating times 0x100", "[combat]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "test_armor"});
    // Disciplined = 0% morale so ResolveCombatStat matches weapon/armour only.
    attacker.SetXp(2);
    defender.SetXp(2);

    const MoraleCalculator morale(*fixture.dataContext.moraleConfig);
    const EffectContext_t attackCtx{&defender.GetTile(), CombatRole_t::Attacker};
    const EffectContext_t defenseCtx{&defender.GetTile(), CombatRole_t::Defender};
    const int attackRating =
        morale.ResolveCombatStat(attacker, StatId_t::Attack, attackCtx);
    const int defenseRating =
        morale.ResolveCombatStat(defender, StatId_t::Defense, defenseCtx);

    CombatHarness_ harness(fixture, /*seed*/ 1);
    const CombatResult_t result = harness.combat.Resolve(attacker, defender);

    CHECK(result.attackStrength == attackRating * CombatResolver::k_combatStrengthScale);
    CHECK(result.defenseStrength == defenseRating * CombatResolver::k_combatStrengthScale);
    CHECK_FALSE(result.rounds.empty());
}

TEST_CASE("Higher roll wins the round; ties go to the defender", "[combat]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    // Attack 4 vs Defense 0 → defender strength 0 always rolls 0; attacker wins unless it
    // also rolls 0 (tie → defender).
    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    const UnitId_t defenderId = defender.GetUnitId();

    CombatHarness_ harness(fixture, /*seed*/ 42);
    const CombatResult_t result = harness.combat.Resolve(attacker, defender);

    REQUIRE_FALSE(result.rounds.empty());
    for (const CombatRound_t& round : result.rounds)
    {
        if (round.attackRoll > round.defenseRoll)
        {
            CHECK(round.roundWinner == CombatSide_t::Attacker);
        }
        else
        {
            CHECK(round.roundWinner == CombatSide_t::Defender);
        }
        CHECK(round.damage == CombatResolver::k_roundDamage);
    }

    CHECK(result.bDefenderDestroyed);
    CHECK_FALSE(result.bAttackerDestroyed);
    CHECK(result.victor == CombatSide_t::Attacker);
    CHECK(result.defenderId == defenderId);
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(5, 4)).empty());
}

TEST_CASE("SingleUse attacker is expended after combat even on a win", "[combat][single-use]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_single_use_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    REQUIRE(attacker.GetFlag(RuleFlagId_t::SingleUse));

    // Expenditure is on UnitOrderExecutor::TryAttack, not CombatResolver::Resolve.
    OrderHarness_ harness(fixture, /*seed*/ 42);
    const auto result = harness.orders.TryAttack(attacker, defender.GetTile());
    REQUIRE(result.has_value());

    CHECK(result->bDefenderDestroyed);
    CHECK(result->bAttackerDestroyed);
    CHECK(result->victor == CombatSide_t::Attacker);
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(4, 4)).empty());
    CHECK(fixture.map.GetUnitsOnTile(fixture.At(5, 4)).empty());
}

TEST_CASE("Zero vs zero combat: ties favour the defender until attacker dies", "[combat]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis"});
    Unit& defender = fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    const int attackerHp = attacker.GetCurrentHp();

    CombatHarness_ harness(fixture, /*seed*/ 7);
    const CombatResult_t result = harness.combat.Resolve(attacker, defender);

    CHECK(result.attackStrength == 0);
    CHECK(result.defenseStrength == 0);
    CHECK(result.rounds.size() == static_cast<size_t>(attackerHp));
    for (const CombatRound_t& round : result.rounds)
    {
        CHECK(round.attackRoll == 0);
        CHECK(round.defenseRoll == 0);
        CHECK(round.roundWinner == CombatSide_t::Defender);
    }
    CHECK(result.bAttackerDestroyed);
    CHECK_FALSE(result.bDefenderDestroyed);
    CHECK(result.victor == CombatSide_t::Defender);
}

TEST_CASE("Each round deals 1 damage and records HP snapshots", "[combat]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});
    const int startAtkHp = attacker.GetCurrentHp();
    const int startDefHp = defender.GetCurrentHp();

    CombatHarness_ harness(fixture, /*seed*/ 99);
    const CombatResult_t result = harness.combat.Resolve(attacker, defender);

    REQUIRE(result.bDefenderDestroyed);
    REQUIRE_FALSE(result.rounds.empty());

    int walkAtk = startAtkHp;
    int walkDef = startDefHp;
    for (const CombatRound_t& round : result.rounds)
    {
        if (round.roundWinner == CombatSide_t::Attacker)
        {
            walkDef -= round.damage;
        }
        else
        {
            walkAtk -= round.damage;
        }
        CHECK(round.attackerHpAfter == walkAtk);
        CHECK(round.defenderHpAfter == walkDef);
    }
    CHECK(walkDef == 0);
    CHECK(walkAtk == attacker.GetCurrentHp());
}

TEST_CASE("Tile defense multiplier scales defender strength", "[combat]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& attacker = fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "test_armor"});

    fixture.At(5, 4).SetRockiness(Rockiness_t::Rocky);
    const double mult =
        fixture.ctx->ResolveTileDefenseMultiplier(fixture.At(5, 4), enemy.GetFactionId());
    REQUIRE(mult == Catch::Approx(1.25));

    const int defenseRating = ResolveStat(defender, StatId_t::Defense);

    CombatHarness_ harness(fixture, /*seed*/ 3);
    const CombatResult_t result = harness.combat.Resolve(attacker, defender);

    CHECK(result.defenseStrength
          == static_cast<int>(
              std::lround(defenseRating * mult * CombatResolver::k_combatStrengthScale)));
}

TEST_CASE("Psi combat ignores additive ratings and applies multipliers to one", "[combat][psi]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& attacker = fixture.MakeUnit(
        player, 4, 4,
        {"test_chassis", "test_weapon", "test_psi", "test_psi_attack_modifiers"});
    Unit& defender = fixture.MakeUnit(
        enemy, 5, 4,
        {"test_chassis", "test_armor", "test_psi_defense_modifiers"});
    // Disciplined = 0% morale so psi percents are unmodified by rank.
    attacker.SetXp(2);
    defender.SetXp(2);

    // Conventional values are huge (156 attack, 206 defense), but psi starts both at 1:
    // attacker +50% => 1.5; defender geometric x2 => 2.
    REQUIRE(ResolveStat(attacker, StatId_t::Attack) == 156);
    REQUIRE(ResolveStat(defender, StatId_t::Defense) == 206);

    CombatHarness_ harness(fixture, /*seed*/ 23);
    const CombatResult_t result = harness.combat.Resolve(attacker, defender);

    CHECK(result.bPsiCombat);
    CHECK(result.attackStrength == 384); // 1.5 * 0x100
    CHECK(result.defenseStrength == 512); // 2.0 * 0x100
}

TEST_CASE("Either combatant can force psi combat", "[combat][psi]")
{
    SECTION("attacker forces psi")
    {
        FactionFixture fixture;
        FillLand_(fixture);
        Faction& player = fixture.MakeFaction();
        Faction& enemy = fixture.MakeFaction();
        Unit& attacker =
            fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon", "test_psi"});
        Unit& defender =
            fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "test_armor"});
        attacker.SetXp(2);
        defender.SetXp(2);

        CombatHarness_ harness(fixture, /*seed*/ 5);
        const CombatResult_t result = harness.combat.Resolve(attacker, defender);
        CHECK(result.bPsiCombat);
        CHECK(result.attackStrength == CombatResolver::k_combatStrengthScale);
        CHECK(result.defenseStrength == CombatResolver::k_combatStrengthScale);
    }

    SECTION("defender forces psi")
    {
        FactionFixture fixture;
        FillLand_(fixture);
        Faction& player = fixture.MakeFaction();
        Faction& enemy = fixture.MakeFaction();
        Unit& attacker =
            fixture.MakeUnit(player, 4, 4, {"test_chassis", "test_weapon"});
        Unit& defender =
            fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "test_armor", "test_psi"});
        attacker.SetXp(2);
        defender.SetXp(2);

        CombatHarness_ harness(fixture, /*seed*/ 5);
        const CombatResult_t result = harness.combat.Resolve(attacker, defender);
        CHECK(result.bPsiCombat);
        CHECK(result.attackStrength == CombatResolver::k_combatStrengthScale);
        CHECK(result.defenseStrength == CombatResolver::k_combatStrengthScale);
    }
}

TEST_CASE("Psi round damage equals the receiving unit's reactor tier", "[combat][psi]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& attacker = fixture.MakeUnit(
        player, 4, 4, {"test_chassis", "test_weapon", "test_psi", "test_psi_tier_2"});
    Unit& defender = fixture.MakeUnit(enemy, 5, 4, {"test_chassis", "test_armor"});

    CombatHarness_ harness(fixture, /*seed*/ 17);
    const CombatResult_t result = harness.combat.Resolve(attacker, defender);

    REQUIRE(result.bPsiCombat);
    REQUIRE_FALSE(result.rounds.empty());
    for (const CombatRound_t& round : result.rounds)
    {
        // The winner deals damage according to the receiver's reactor: the tier-2 attacker
        // takes 2 when the defender wins; the default-tier defender takes 1.
        const int expected = round.roundWinner == CombatSide_t::Attacker ? 1 : 2;
        CHECK(round.damage == expected);
    }
}

TEST_CASE("Conventional combat keeps fixed one damage despite psi tier", "[combat]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    Unit& attacker = fixture.MakeUnit(
        player, 4, 4, {"test_chassis", "test_weapon", "test_psi_tier_2"});
    Unit& defender = fixture.MakeUnit(enemy, 5, 4, {"test_chassis"});

    CombatHarness_ harness(fixture, /*seed*/ 9);
    const CombatResult_t result = harness.combat.Resolve(attacker, defender);

    CHECK_FALSE(result.bPsiCombat);
    for (const CombatRound_t& round : result.rounds)
    {
        CHECK(round.damage == CombatResolver::k_roundDamage);
    }
}

// --- Disengage ---------------------------------------------------------------------------
//
// Baseline eligible defender: a combat unit (test_weapon), strictly faster than the slow
// attacker (2 vs 1 movement), alone on its tile, no attack history, no hold order, on open
// land. Attack 4 vs Defense 0 means the defender loses HP nearly every round and hits the
// 50% threshold long before dying.
//
// Baseline eligible attacker: fast chassis + weak weapon vs a slow, heavily armored defender
// so the attacker loses rounds and can withdraw.

namespace
{

struct DisengageSetup_
{
    FactionFixture fixture;
    Faction* pPlayer = nullptr;
    Faction* pEnemy = nullptr;
    Unit* pAttacker = nullptr;
    Unit* pDefender = nullptr;

    explicit DisengageSetup_(std::vector<std::string> attackerParts = {"test_slow_chassis",
                                                                       "test_weapon"},
                             std::vector<std::string> defenderParts = {"test_chassis",
                                                                       "test_weapon"})
    {
        FillLand_(fixture);
        pPlayer = &fixture.MakeFaction();
        pEnemy = &fixture.MakeFaction();
        pAttacker = &fixture.MakeUnit(*pPlayer, 5, 4, attackerParts);
        pDefender = &fixture.MakeUnit(*pEnemy, 4, 4, defenderParts);
    }
};

} // namespace

TEST_CASE("Eligible defender disengages at half HP to an adjacent tile", "[combat][disengage]")
{
    DisengageSetup_ setup;
    const int startHp = setup.pDefender->GetCurrentHp();

    CombatHarness_ harness(setup.fixture, /*seed*/ 11);
    const CombatResult_t result = harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);

    REQUIRE(result.bDefenderDisengaged);
    CHECK_FALSE(result.bAttackerDisengaged);
    CHECK_FALSE(result.bDefenderDestroyed);
    CHECK_FALSE(result.bAttackerDestroyed);
    CHECK(result.victor == CombatSide_t::Attacker);

    // Withdrew exactly when its combat damage reached half its starting HP.
    CHECK((startHp - setup.pDefender->GetCurrentHp()) * 2 >= startHp);
    CHECK(setup.pDefender->GetCurrentHp() > 0);

    // Moved off its tile to the reported retreat tile — never adjacent to the attacker
    // (those tiles are ZOC→ZOC violations from the defender's position).
    REQUIRE(result.pRetreatTile != nullptr);
    CHECK(&setup.pDefender->GetTile() == result.pRetreatTile);
    CHECK(result.pRetreatTile->GetX() == 3);
}

TEST_CASE("Eligible attacker disengages at half HP to an adjacent tile", "[combat][disengage]")
{
    // Fast attacker (move 2) with weak gun vs slow armored defender — attacker loses rounds.
    DisengageSetup_ setup({"test_chassis", "test_weak_weapon"},
                          {"test_slow_chassis", "test_armor"});
    const int startHp = setup.pAttacker->GetCurrentHp();
    const int attackerX = setup.pAttacker->GetTile().GetX();

    CombatHarness_ harness(setup.fixture, /*seed*/ 11);
    const CombatResult_t result = harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);

    REQUIRE(result.bAttackerDisengaged);
    CHECK_FALSE(result.bDefenderDisengaged);
    CHECK_FALSE(result.bAttackerDestroyed);
    CHECK_FALSE(result.bDefenderDestroyed);
    CHECK(result.victor == CombatSide_t::Defender);

    CHECK((startHp - setup.pAttacker->GetCurrentHp()) * 2 >= startHp);
    CHECK(setup.pAttacker->GetCurrentHp() > 0);
    REQUIRE(result.pRetreatTile != nullptr);
    CHECK(&setup.pAttacker->GetTile() == result.pRetreatTile);
    CHECK(result.pRetreatTile->GetX() != attackerX);
}

TEST_CASE("Disengage criteria each block withdrawal", "[combat][disengage]")
{
    SECTION("defender not faster than attacker")
    {
        DisengageSetup_ setup({"test_chassis", "test_weapon"}); // same speed
        CombatHarness_ harness(setup.fixture, /*seed*/ 11);
        const CombatResult_t result =
            harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);
        CHECK_FALSE(result.bDefenderDisengaged);
        CHECK(result.bDefenderDestroyed);
    }

    SECTION("opponent has PreventsDisengage (Comm Jammer)")
    {
        DisengageSetup_ setup({"test_slow_chassis", "test_weapon", "comm_jammer"});
        CombatHarness_ harness(setup.fixture, /*seed*/ 11);
        const CombatResult_t result =
            harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);
        CHECK_FALSE(result.bDefenderDisengaged);
        CHECK(result.bDefenderDestroyed);
    }

    SECTION("defender's Comm Jammer blocks attacker disengage")
    {
        DisengageSetup_ setup({"test_chassis", "test_weak_weapon"},
                              {"test_slow_chassis", "test_armor", "comm_jammer"});
        CombatHarness_ harness(setup.fixture, /*seed*/ 11);
        const CombatResult_t result =
            harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);
        CHECK_FALSE(result.bAttackerDisengaged);
        CHECK(result.bAttackerDestroyed);
    }

    SECTION("non-combat defender (attack 0)")
    {
        DisengageSetup_ setup;
        Unit& scout = setup.fixture.MakeUnit(*setup.pEnemy, 4, 5, {"test_chassis"});
        CombatHarness_ harness(setup.fixture, /*seed*/ 11);
        const CombatResult_t result = harness.combat.Resolve(*setup.pAttacker, scout);
        CHECK_FALSE(result.bDefenderDisengaged);
    }

    SECTION("defender stacked with another unit")
    {
        DisengageSetup_ setup;
        setup.fixture.MakeUnit(*setup.pEnemy, 4, 4, {"test_chassis"});
        CombatHarness_ harness(setup.fixture, /*seed*/ 11);
        const CombatResult_t result =
            harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);
        CHECK_FALSE(result.bDefenderDisengaged);
        CHECK(result.bDefenderDestroyed);
    }

    SECTION("defender attacked this turn")
    {
        DisengageSetup_ setup;
        setup.pDefender->MarkAttacked();
        CombatHarness_ harness(setup.fixture, /*seed*/ 11);
        const CombatResult_t result =
            harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);
        CHECK_FALSE(result.bDefenderDisengaged);
    }

    SECTION("defender attacked last turn")
    {
        DisengageSetup_ setup;
        setup.pDefender->MarkAttacked();
        setup.pDefender->AdvanceAttackHistory();
        CombatHarness_ harness(setup.fixture, /*seed*/ 11);
        const CombatResult_t result =
            harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);
        CHECK_FALSE(result.bDefenderDisengaged);
    }

    SECTION("attack two turns ago no longer blocks")
    {
        DisengageSetup_ setup;
        setup.pDefender->MarkAttacked();
        setup.pDefender->AdvanceAttackHistory();
        setup.pDefender->AdvanceAttackHistory();
        CombatHarness_ harness(setup.fixture, /*seed*/ 11);
        const CombatResult_t result =
            harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);
        CHECK(result.bDefenderDisengaged);
    }

    SECTION("defender under a Hold-family order")
    {
        DisengageSetup_ setup;
        setup.pDefender->SetOrder(HoldOrder_t{});
        CombatHarness_ harness(setup.fixture, /*seed*/ 11);
        const CombatResult_t result =
            harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);
        CHECK_FALSE(result.bDefenderDisengaged);
    }

    SECTION("defender on a tile with a prevents_disengage feature (Bunker)")
    {
        DisengageSetup_ setup;
        setup.fixture.ctx->AddImprovementWithEffects(setup.fixture.At(4, 4), "Bunker");
        CombatHarness_ harness(setup.fixture, /*seed*/ 11);
        const CombatResult_t result =
            harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);
        CHECK_FALSE(result.bDefenderDisengaged);
    }
}

TEST_CASE("Air units never take part in disengage", "[combat][disengage]")
{
    FactionFixture fixture;
    FillLand_(fixture);
    Faction& player = fixture.MakeFaction();
    Faction& enemy = fixture.MakeFaction();

    // A fast air defender over a slow land attacker still refuses to disengage.
    Unit& attacker = fixture.MakeUnit(player, 5, 4, {"test_slow_chassis", "test_weapon"});
    Unit& defender = fixture.MakeUnit(enemy, 4, 4, {"test_flight_chassis", "test_weapon"});

    CombatHarness_ harness(fixture, /*seed*/ 11);
    const CombatResult_t result = harness.combat.Resolve(attacker, defender);
    CHECK_FALSE(result.bDefenderDisengaged);
    CHECK_FALSE(result.bAttackerDisengaged);
}

TEST_CASE("No valid retreat square means fighting on", "[combat][disengage]")
{
    DisengageSetup_ setup;
    // Water everywhere except the two combat tiles: a land defender has nowhere to go.
    FillWater_(setup.fixture);
    setup.fixture.At(4, 4).SetElevation(100);
    setup.fixture.At(5, 4).SetElevation(100);

    CombatHarness_ harness(setup.fixture, /*seed*/ 11);
    const CombatResult_t result = harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);
    CHECK_FALSE(result.bDefenderDisengaged);
    CHECK(result.bDefenderDestroyed);
}

TEST_CASE("Fungus blocks retreat unless it has a road", "[combat][disengage]")
{
    SECTION("bare fungus excluded")
    {
        DisengageSetup_ setup;
        FillWater_(setup.fixture);
        setup.fixture.At(4, 4).SetElevation(100);
        setup.fixture.At(5, 4).SetElevation(100);
        setup.fixture.At(3, 4).SetElevation(100);
        setup.fixture.At(3, 4).SetHasFungus(true);

        CombatHarness_ harness(setup.fixture, /*seed*/ 11);
        const CombatResult_t result =
            harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);
        CHECK_FALSE(result.bDefenderDisengaged);
    }

    SECTION("fungus with a road admits the retreat")
    {
        DisengageSetup_ setup;
        FillWater_(setup.fixture);
        setup.fixture.At(4, 4).SetElevation(100);
        setup.fixture.At(5, 4).SetElevation(100);
        setup.fixture.At(3, 4).SetElevation(100);
        setup.fixture.At(3, 4).SetHasFungus(true);
        setup.fixture.At(3, 4).AddImprovement(setup.fixture.improvements.Get("Road"));

        CombatHarness_ harness(setup.fixture, /*seed*/ 11);
        const CombatResult_t result =
            harness.combat.Resolve(*setup.pAttacker, *setup.pDefender);
        REQUIRE(result.bDefenderDisengaged);
        CHECK(result.pRetreatTile == &setup.fixture.At(3, 4));
        CHECK(&setup.pDefender->GetTile() == &setup.fixture.At(3, 4));
    }
}

#include "DiplomacyFixture.h"

#include "game/diplomacy/DiplomaticProposalMutations.h"
#include "game/diplomacy/DiplomaticStandingMutations.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/economy/EconomyManager.h"
#include "game/faction/Faction.h"
#include "game/units/UnitAttackExecutor.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace actest;

TEST_CASE("ApplyFriction adds and clamps, directed", "[diplomacy][standing]")
{
    DiplomacyFixture game;
    const FactionId_t a = game.pA->GetFactionId();
    const FactionId_t b = game.pB->GetFactionId();

    ApplyFriction(game.fixtures.state, a, b, 19);
    CHECK(game.Ledger().GetFriction(a, b) == 19);
    CHECK(game.Ledger().GetFriction(b, a) == 0);

    ApplyFriction(game.fixtures.state, a, b, 5);
    CHECK(game.Ledger().GetFriction(a, b) == 20);

    ApplyFriction(game.fixtures.state, a, b, -100);
    CHECK(game.Ledger().GetFriction(a, b) == 0);
}

TEST_CASE("ApplyBlemishes adds and clamps", "[diplomacy][standing]")
{
    DiplomacyFixture game;
    const FactionId_t a = game.pA->GetFactionId();

    ApplyBlemishes(game.fixtures.state, a, BlemishChange_t{.delta = 98});
    CHECK(game.Ledger().GetBlemishes(a) == 98);

    ApplyBlemishes(game.fixtures.state, a, BlemishChange_t{.delta = 5});
    CHECK(game.Ledger().GetBlemishes(a) == 99);
    CHECK(game.Ledger().GetBlemishes(game.pB->GetFactionId()) == 0);
}

TEST_CASE("ApplyBlemishes raises to the floor then clamps", "[diplomacy][standing]")
{
    DiplomacyFixture game;
    const FactionId_t a = game.pA->GetFactionId();

    ApplyBlemishes(game.fixtures.state, a, BlemishChange_t{.delta = 2, .floor = 6});
    CHECK(game.Ledger().GetBlemishes(a) == 6);

    ApplyBlemishes(game.fixtures.state, a, BlemishChange_t{.delta = 2, .floor = 150});
    CHECK(game.Ledger().GetBlemishes(a) == 99);
}

TEST_CASE("Cancelling an agreement blemishes the canceller", "[diplomacy][standing][cancel]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);

    CancelTreaty(game.fixtures.state, game.pA->GetFactionId(), game.pB->GetFactionId());
    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 1);
    CHECK(game.Ledger().GetBlemishes(game.pB->GetFactionId()) == 0);

    CancelTreaty(game.fixtures.state, game.pB->GetFactionId(), game.pA->GetFactionId());
    CHECK(game.Ledger().GetBlemishes(game.pB->GetFactionId()) == 1);
}

TEST_CASE("A Neutral attack is not an agreement break", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;

    ApplyHostileAct(game.fixtures.state, HostileAct_t{.aggressor = game.pA->GetFactionId(),
                                                      .victim = game.pB->GetFactionId(),
                                                      .bAttributed = true});

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 0);
}

TEST_CASE("A sneak attack blemishes the aggressor", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);

    ApplyHostileAct(game.fixtures.state, HostileAct_t{.aggressor = game.pA->GetFactionId(),
                                                      .victim = game.pB->GetFactionId(),
                                                      .bAttributed = true});

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 1);
    CHECK(game.Ledger().GetBlemishes(game.pB->GetFactionId()) == 0);
}

TEST_CASE("An unattributed hostile act adds no blemishes", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);

    ApplyHostileAct(game.fixtures.state, HostileAct_t{.aggressor = game.pA->GetFactionId(),
                                                      .victim = game.pB->GetFactionId(),
                                                      .bAttributed = false});

    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 0);
}

TEST_CASE("Fighting at Vendetta adds no blemishes", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);

    ApplyHostileAct(game.fixtures.state, HostileAct_t{.aggressor = game.pA->GetFactionId(),
                                                      .victim = game.pB->GetFactionId(),
                                                      .bAttributed = true});

    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 0);
}

TEST_CASE("A declared Vendetta from Neutral adds no blemishes", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;
    DeclareVendetta(game.fixtures.state, game.pA->GetFactionId(), game.pB->GetFactionId());
    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 0);
}

TEST_CASE("Declaring Vendetta from Treaty blemishes the declarer", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);

    DeclareVendetta(game.fixtures.state, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 1);
    CHECK(game.Ledger().GetBlemishes(game.pB->GetFactionId()) == 0);
}

TEST_CASE("Declaring Vendetta from Pact blemishes the declarer", "[diplomacy][standing][hostile]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);

    DeclareVendetta(game.fixtures.state, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 1);
}

TEST_CASE("An atrocity Vendetta from Treaty adds no agreement-break blemish",
          "[diplomacy][standing][atrocity]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);

    ApplyAtrocityVendetta(game.fixtures.state, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 0);
}

TEST_CASE("Honoring a defensive obligation adds no blemishes",
          "[diplomacy][standing][obligation]")
{
    DiplomacyFixture game;
    game.Set(*game.pC, *game.pA, DiplomaticStatus_t::Treaty);

    HonorDefensiveObligation(game.fixtures.state, game.pC->GetFactionId(), game.pA->GetFactionId(),
                             VendettaKind_t::Declaration);

    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Ledger().GetBlemishes(game.pC->GetFactionId()) == 0);
}

TEST_CASE("A sneak against native life adds no blemishes", "[diplomacy][standing][native]")
{
    DiplomacyFixture game;
    Faction& rPlanet = AddNativeLifeFaction(game.fixtures);

    ApplyHostileAct(game.fixtures.state, HostileAct_t{.aggressor = game.pA->GetFactionId(),
                                                      .victim = rPlanet.GetFactionId(),
                                                      .bAttributed = true});

    CHECK(game.Ledger().GetBlemishes(game.pA->GetFactionId()) == 0);
}

namespace
{

// A at Truce with B, B at Vendetta with C, so A fighting C on B's land earns B's goodwill.
// Returns the battle tile (11, 9), which B owns.
Tile& SetUpAlliedTerritory_(DiplomacyFixture& rGame)
{
    rGame.Set(*rGame.pA, *rGame.pB, DiplomaticStatus_t::Truce);
    rGame.Set(*rGame.pB, *rGame.pC, DiplomaticStatus_t::Vendetta);
    REQUIRE(rGame.Owner(11, 9) == rGame.pB->GetFactionId());
    rGame.Ledger().SetFriction(rGame.pB->GetFactionId(), rGame.pA->GetFactionId(), 5);
    return rGame.At(11, 9);
}

int TerritoryFriction_(DiplomacyFixture& rGame)
{
    return rGame.fixtures.state.GetGameData().diplomacyConfig->alliedTerritoryBattleFriction;
}

int FrictionBTowardA_(DiplomacyFixture& rGame)
{
    return rGame.Ledger().GetFriction(rGame.pB->GetFactionId(), rGame.pA->GetFactionId());
}

} // namespace

TEST_CASE("Fighting the owner's Vendetta enemy on its land changes its friction toward the "
          "attacker",
          "[diplomacy][standing][combat]")
{
    DiplomacyFixture game;
    SetUpAlliedTerritory_(game);
    Unit& rAttacker = game.MakeUnit(*game.pA, 10, 8, {"test_chassis", "test_weapon"});
    Unit& rDefender = game.MakeUnit(*game.pC, 11, 9);

    REQUIRE(game.fixtures.state.GetUnitAttacks().TryAttack(rAttacker, rDefender));

    CHECK(FrictionBTowardA_(game) == 5 + TerritoryFriction_(game));
}

TEST_CASE("Allied-territory friction applies when the attacker dies",
          "[diplomacy][standing][combat]")
{
    DiplomacyFixture game;
    SetUpAlliedTerritory_(game);
    Unit& rAttacker = game.MakeUnit(*game.pA, 10, 8, {"test_chassis", "test_weapon"});
    rAttacker.SetCurrentHp(1);
    Unit& rDefender = game.MakeUnit(*game.pC, 11, 9, {"test_chassis", "test_armor"});

    const auto result = game.fixtures.state.GetUnitAttacks().TryAttack(rAttacker, rDefender);
    REQUIRE(result);
    REQUIRE(result->bAttackerDestroyed);

    CHECK(FrictionBTowardA_(game) == 5 + TerritoryFriction_(game));
}

TEST_CASE("Allied-territory friction clamps at zero", "[diplomacy][standing][combat]")
{
    DiplomacyFixture game;
    SetUpAlliedTerritory_(game);
    game.Ledger().SetFriction(game.pB->GetFactionId(), game.pA->GetFactionId(), 0);
    Unit& rAttacker = game.MakeUnit(*game.pA, 10, 8, {"test_chassis", "test_weapon"});
    Unit& rDefender = game.MakeUnit(*game.pC, 11, 9);

    REQUIRE(game.fixtures.state.GetUnitAttacks().TryAttack(rAttacker, rDefender));

    CHECK(FrictionBTowardA_(game) == 0);
}

TEST_CASE("Allied-territory friction needs the owner at Vendetta with the defender and in an "
          "agreement with the attacker",
          "[diplomacy][standing][combat]")
{
    DiplomacyFixture game;
    Tile& rBattle = SetUpAlliedTerritory_(game);
    const FactionId_t a = game.pA->GetFactionId();
    const FactionId_t b = game.pB->GetFactionId();
    const FactionId_t c = game.pC->GetFactionId();

    CHECK(AlliedTerritoryOwner(a, c, rBattle) == b);
    CHECK_FALSE(AlliedTerritoryOwner(b, c, rBattle));
    CHECK_FALSE(AlliedTerritoryOwner(a, b, rBattle));
    CHECK_FALSE(AlliedTerritoryOwner(a, c, game.At(0, 16)));

    game.Ledger().SetStatus(b, c, DiplomaticStatus_t::Neutral);
    CHECK_FALSE(AlliedTerritoryOwner(a, c, rBattle));

    game.Ledger().SetStatus(b, c, DiplomaticStatus_t::Vendetta);
    game.Ledger().SetStatus(a, b, DiplomaticStatus_t::Neutral);
    CHECK_FALSE(AlliedTerritoryOwner(a, c, rBattle));

    game.Ledger().SetStatus(a, b, DiplomaticStatus_t::Pact);
    CHECK(AlliedTerritoryOwner(a, c, rBattle) == b);
}

TEST_CASE("Allied-territory friction counts the owner's water as well as its land",
          "[diplomacy][standing][combat]")
{
    DiplomacyFixture game;
    Tile& rBattle = SetUpAlliedTerritory_(game);
    rBattle.SetElevation(-500);
    REQUIRE(rBattle.IsWater());
    REQUIRE(game.Owner(11, 9) == game.pB->GetFactionId());

    CHECK(AlliedTerritoryOwner(game.pA->GetFactionId(), game.pC->GetFactionId(), rBattle) ==
          game.pB->GetFactionId());
}

TEST_CASE("An artillery duel on allied territory writes friction; a bombard strike does not",
          "[diplomacy][standing][combat][bombard]")
{
    SECTION("strike")
    {
        DiplomacyFixture game;
        Tile& rBattle = SetUpAlliedTerritory_(game);
        Unit& rShooter = game.MakeUnit(*game.pA, 10, 8, {"test_chassis", "bombard"});
        game.MakeUnit(*game.pC, 11, 9);

        REQUIRE(game.fixtures.state.GetUnitAttacks().TryBombard(rShooter, rBattle));

        CHECK(FrictionBTowardA_(game) == 5);
    }
    SECTION("duel")
    {
        DiplomacyFixture game;
        Tile& rBattle = SetUpAlliedTerritory_(game);
        Unit& rShooter = game.MakeUnit(*game.pA, 10, 8, {"test_chassis", "test_weapon", "bombard"});
        game.MakeUnit(*game.pC, 11, 9, {"test_slow_chassis", "bombard"});

        const auto result = game.fixtures.state.GetUnitAttacks().TryBombard(rShooter, rBattle);
        REQUIRE(result);
        REQUIRE(result->combats.size() == 1);

        CHECK(FrictionBTowardA_(game) == 5 + TerritoryFriction_(game));
    }
}

TEST_CASE("A battle on allied ground floors the attacker's gift count toward the owner",
          "[diplomacy][standing][combat][gifts]")
{
    DiplomacyFixture game;
    Tile& rBattle = SetUpAlliedTerritory_(game);
    const FactionId_t a = game.pA->GetFactionId();
    const FactionId_t b = game.pB->GetFactionId();
    const FactionId_t c = game.pC->GetFactionId();

    ApplyAlliedTerritoryFriction(game.fixtures.state, a, c, rBattle);
    CHECK(game.Ledger().GetGifts(a, b) == 1);

    game.Ledger().SetGifts(a, b, 3);
    ApplyAlliedTerritoryFriction(game.fixtures.state, a, c, rBattle);
    CHECK(game.Ledger().GetGifts(a, b) == 3);
}

TEST_CASE("ApplyGiftStanding writes the recipient's friction and the giver's count",
          "[diplomacy][standing][gifts]")
{
    DiplomacyFixture game;
    const FactionId_t a = game.pA->GetFactionId();
    const FactionId_t b = game.pB->GetFactionId();
    game.Ledger().SetFriction(b, a, 10);
    game.Ledger().SetBlemishes(a, 5);

    ApplyGiftStanding(game.fixtures.state, a, b, TradeCredits_t{100});

    CHECK(game.Ledger().GetFriction(b, a) == 7);
    CHECK(game.Ledger().GetGifts(a, b) == 2);
    CHECK(game.Ledger().GetFriction(a, b) == 0);
    CHECK(game.Ledger().GetGifts(b, a) == 0);
}

TEST_CASE("A gift values each item just before it moves", "[diplomacy][standing][gifts]")
{
    DiplomacyFixture game;
    const FactionId_t a = game.pA->GetFactionId();
    const FactionId_t b = game.pB->GetFactionId();
    game.Ledger().SetFriction(b, a, 10);
    game.Ledger().SetBlemishes(a, 5);
    game.pA->GetEconomy().AddEnergy(400);
    DiplomaticProposal_t gift;
    gift.proposer = a;
    gift.recipient = b;
    gift.give = {TradeCredits_t{100}, TradeCredits_t{100}};

    ApplyProposal(game.fixtures.state, gift);

    // The first 100 is short of a third of 400, so notable; the second meets a third of 300.
    CHECK(game.Ledger().GetGifts(a, b) == 3);
    CHECK(game.Ledger().GetFriction(b, a) == 4);
    CHECK(game.pB->GetEconomy().GetEnergy() == 200);
}

TEST_CASE("A base gift is valued while the giver still holds it",
          "[diplomacy][standing][gifts][base_value]")
{
    DiplomacyFixture game;
    const FactionId_t a = game.pA->GetFactionId();
    const FactionId_t b = game.pB->GetFactionId();
    game.Ledger().SetFriction(b, a, 10);
    game.Ledger().SetBlemishes(a, 5);
    BaseManager& rGiven = MakeSessionBase(game.fixtures, *game.pA, 20, 6);
    const BaseId_t givenId = rGiven.GetBaseId();
    DiplomaticProposal_t gift;
    gift.proposer = a;
    gift.recipient = b;
    gift.give = {TradeBase_t{givenId}};

    ApplyProposal(game.fixtures.state, gift);

    CHECK(game.pB->FindBase(givenId) != nullptr);
    CHECK(game.Ledger().GetFriction(b, a) == 0);
    CHECK(game.Ledger().GetGifts(a, b) == 2);
}

TEST_CASE("A trade that asks something back writes no gift standing",
          "[diplomacy][standing][gifts]")
{
    DiplomacyFixture game;
    const FactionId_t a = game.pA->GetFactionId();
    const FactionId_t b = game.pB->GetFactionId();
    game.Ledger().SetFriction(b, a, 10);
    game.Ledger().SetBlemishes(a, 5);
    game.pA->GetEconomy().AddEnergy(100);
    DiplomaticProposal_t trade;
    trade.proposer = a;
    trade.recipient = b;
    trade.give = {TradeCredits_t{100}};
    trade.demand = {TradeWorldMap_t{}};

    ApplyProposal(game.fixtures.state, trade);

    CHECK(game.pB->GetEconomy().GetEnergy() == 100);
    CHECK(game.Ledger().GetFriction(b, a) == 10);
    CHECK(game.Ledger().GetGifts(a, b) == 0);
}

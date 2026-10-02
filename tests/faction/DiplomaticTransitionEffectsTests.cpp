#include "DiplomacyFixture.h"

#include "game/faction/DiplomacyConfig.h"
#include "game/faction/DiplomaticTransitionEffects.h"
#include "game/units/EvacuateTerritoryEffects.h"
#include "game/units/UnitOrderExecutor.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <vector>

using namespace ac;
using namespace actest;

TEST_CASE("A Truce steps down to Neutral once its duration has passed",
          "[diplomacy][status][expiry]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Truce);
    game.Set(*game.pA, *game.pC, DiplomaticStatus_t::Treaty);

    // The fixture Truce lasts 3 turns.
    ExpireDiplomaticStatuses(*game.pState);
    ExpireDiplomaticStatuses(*game.pState);
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Truce);

    ExpireDiplomaticStatuses(*game.pState);
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Neutral);
    CHECK(game.Status(*game.pA, *game.pC) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("Signing a new status restarts the expiry count", "[diplomacy][status][expiry]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Truce);
    ExpireDiplomaticStatuses(*game.pState);
    ExpireDiplomaticStatuses(*game.pState);

    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Truce);
    ExpireDiplomaticStatuses(*game.pState);
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Truce);
}

TEST_CASE("Signing a Treaty moves each side's units out of the other's territory",
          "[diplomacy][status][evacuate]")
{
    DiplomacyFixture game;
    REQUIRE(game.Owner(6, 4) == game.pB->GetFactionId());
    REQUIRE(game.Owner(2, 4) == game.pA->GetFactionId());
    Unit& rGuest = game.MakeUnit(*game.pA, 6, 4);
    Unit& rVisitor = game.MakeUnit(*game.pB, 2, 4);

    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);

    CHECK(game.Map().GetTerritory().GetOwner(rGuest.GetTile()) == game.pA->GetFactionId());
    CHECK(game.Map().GetTerritory().GetOwner(rVisitor.GetTile()) == game.pB->GetFactionId());
}

TEST_CASE("A Pact ending in Vendetta clears shared tiles and bases but not territory",
          "[diplomacy][status][evacuate]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);

    REQUIRE(game.Owner(6, 3) == game.pB->GetFactionId());
    Unit& rInBase = game.MakeUnit(*game.pA, 7, 4);
    Unit& rStacked = game.MakeUnit(*game.pA, 4, 0);
    Unit& rPartner = game.MakeUnit(*game.pB, 4, 0);
    Unit& rInTerritory = game.MakeUnit(*game.pA, 6, 3);

    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);

    CHECK(&rInBase.GetTile() != &game.pBaseB->GetTile());
    CHECK(&rStacked.GetTile() != &rPartner.GetTile());
    CHECK(&rInTerritory.GetTile() == &game.At(6, 3));
}

TEST_CASE("An evacuated unit is not set down among units it may not share a tile with",
          "[diplomacy][status][evacuate]")
{
    DiplomacyFixture game;
    REQUIRE(game.Owner(4, 4) == game.pA->GetFactionId());
    Unit& rGuest = game.MakeUnit(*game.pB, 4, 4);
    // Occupy every tile of B's on the ring nearest the guest.
    for (int y = 3; y <= 5; ++y)
    {
        REQUIRE(game.Owner(5, y) == game.pB->GetFactionId());
        game.MakeUnit(*game.pA, 5, y);
    }

    EvacuateUnitsFromTerritory(*game.pB, game.pA->GetFactionId(), game.Map(), game.Grids());

    CHECK(game.Map().GetTerritory().GetOwner(rGuest.GetTile()) == game.pB->GetFactionId());
    CHECK_FALSE(game.SharesAnyTile(*game.pA, *game.pB));
}

TEST_CASE("Declaring Vendetta moves each side's units out of the other's territory",
          "[diplomacy][status][evacuate][declaration]")
{
    DiplomacyFixture game;
    REQUIRE(game.Owner(6, 4) == game.pB->GetFactionId());
    REQUIRE(game.Owner(2, 4) == game.pA->GetFactionId());
    Unit& rGuest = game.MakeUnit(*game.pA, 6, 4);
    Unit& rVisitor = game.MakeUnit(*game.pB, 2, 4);

    game.ProposeVendetta(*game.pA, *game.pB);

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Map().GetTerritory().GetOwner(rGuest.GetTile()) == game.pA->GetFactionId());
    CHECK(game.Map().GetTerritory().GetOwner(rVisitor.GetTile()) == game.pB->GetFactionId());
}

TEST_CASE("Declaring Vendetta on a Pact partner clears territory, shared tiles and bases",
          "[diplomacy][status][evacuate][declaration]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);

    REQUIRE(game.Owner(6, 3) == game.pB->GetFactionId());
    Unit& rInBase = game.MakeUnit(*game.pA, 7, 4);
    Unit& rStacked = game.MakeUnit(*game.pA, 4, 0);
    Unit& rPartner = game.MakeUnit(*game.pB, 4, 0);
    Unit& rInTerritory = game.MakeUnit(*game.pA, 6, 3);

    game.ProposeVendetta(*game.pA, *game.pB);

    CHECK(&rInBase.GetTile() != &game.pBaseB->GetTile());
    CHECK(&rStacked.GetTile() != &rPartner.GetTile());
    CHECK(game.Map().GetTerritory().GetOwner(rInTerritory.GetTile()) == game.pA->GetFactionId());
}

TEST_CASE("A sneak attack leaves each side's units in the other's territory",
          "[diplomacy][status][evacuate][sneak]")
{
    DiplomacyFixture game;
    REQUIRE(game.Owner(6, 3) == game.pB->GetFactionId());
    REQUIRE(game.Owner(2, 4) == game.pA->GetFactionId());
    Unit& rGuest = game.MakeUnit(*game.pA, 6, 3);
    Unit& rVisitor = game.MakeUnit(*game.pB, 2, 4);
    Unit& rAttacker = game.MakeUnit(*game.pA, 4, 0, {"test_chassis", "test_weapon"});
    Unit& rDefender = game.MakeUnit(*game.pB, 5, 0);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryAttack(rAttacker, rDefender.GetTile()));

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(&rGuest.GetTile() == &game.At(6, 3));
    CHECK(&rVisitor.GetTile() == &game.At(2, 4));
}

TEST_CASE("A sneak attack on a Pact partner clears shared tiles and bases but not territory",
          "[diplomacy][status][evacuate][sneak]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);

    REQUIRE(game.Owner(6, 3) == game.pB->GetFactionId());
    Unit& rInBase = game.MakeUnit(*game.pA, 7, 4);
    Unit& rStacked = game.MakeUnit(*game.pA, 4, 0);
    Unit& rPartner = game.MakeUnit(*game.pB, 4, 0);
    Unit& rInTerritory = game.MakeUnit(*game.pA, 6, 3);

    ApplyHostileAct(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(&rInBase.GetTile() != &game.pBaseB->GetTile());
    CHECK(&rStacked.GetTile() != &rPartner.GetTile());
    CHECK(&rInTerritory.GetTile() == &game.At(6, 3));
}

TEST_CASE("A sneak attack out of a shared stack resolves, then separates the two factions",
          "[diplomacy][status][evacuate][sneak]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);

    REQUIRE(game.Owner(6, 3) == game.pB->GetFactionId());
    Unit& rAttacker = game.MakeUnit(*game.pA, 4, 0, {"test_chassis", "test_weapon"});
    game.MakeUnit(*game.pB, 4, 0);
    Unit& rDefender = game.MakeUnit(*game.pB, 5, 0);
    game.MakeUnit(*game.pA, 5, 0);
    Unit& rInTerritory = game.MakeUnit(*game.pA, 6, 3);
    const UnitId_t attackerId = rAttacker.GetUnitId();
    const UnitId_t defenderId = rDefender.GetUnitId();

    const std::optional<CombatResult_t> result =
        game.pState->GetUnitOrderExecutor().TryAttack(rAttacker, game.At(5, 0));

    REQUIRE(result);
    CHECK(result->attackerId == attackerId);
    CHECK(result->defenderId == defenderId);
    CHECK_FALSE(result->rounds.empty());
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK_FALSE(game.SharesAnyTile(*game.pA, *game.pB));
    CHECK(&rInTerritory.GetTile() == &game.At(6, 3));
}

TEST_CASE("Attacking a faction you are not at Vendetta with declares Vendetta",
          "[diplomacy][status][hostile]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    Unit& rAttacker = game.MakeUnit(*game.pA, 4, 0, {"test_chassis", "test_weapon"});
    Unit& rDefender = game.MakeUnit(*game.pB, 5, 0);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryAttack(rAttacker, rDefender.GetTile()));
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
}

TEST_CASE("Bombarding a faction declares Vendetta", "[diplomacy][status][hostile]")
{
    DiplomacyFixture game;
    Unit& rAttacker = game.MakeUnit(*game.pA, 4, 0, {"test_chassis", "bombard"});
    game.MakeUnit(*game.pB, 5, 0);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryBombard(rAttacker, game.At(5, 0)));
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
}

TEST_CASE("An AI Pact partner of the victim declares Vendetta on the aggressor",
          "[diplomacy][status][obligation]")
{
    DiplomacyFixture game;
    game.Set(*game.pB, *game.pC, DiplomaticStatus_t::Pact);

    ApplyHostileAct(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Status(*game.pB, *game.pC) == DiplomaticStatus_t::Pact);
}

TEST_CASE("A Pact partner defending against a declaration leaves the aggressor's territory",
          "[diplomacy][status][obligation][declaration]")
{
    DiplomacyFixture game;
    game.Set(*game.pB, *game.pC, DiplomaticStatus_t::Pact);
    MakeSessionBase(game.fixtures, *game.pState, *game.pC, 4, 8);
    REQUIRE(game.Owner(2, 4) == game.pA->GetFactionId());
    Unit& rDefenderGuest = game.MakeUnit(*game.pC, 2, 4);

    game.ProposeVendetta(*game.pA, *game.pB);

    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Map().GetTerritory().GetOwner(rDefenderGuest.GetTile())
          != game.pA->GetFactionId());
}

TEST_CASE("A Pact partner defending against a sneak attack stays in the aggressor's territory",
          "[diplomacy][status][obligation][sneak]")
{
    DiplomacyFixture game;
    game.Set(*game.pB, *game.pC, DiplomaticStatus_t::Pact);
    REQUIRE(game.Owner(2, 4) == game.pA->GetFactionId());
    Unit& rDefenderGuest = game.MakeUnit(*game.pC, 2, 4);

    ApplyHostileAct(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Vendetta);
    CHECK(&rDefenderGuest.GetTile() == &game.At(2, 4));
}

TEST_CASE("The player's obligation after a sneak attack keeps it a sneak attack",
          "[diplomacy][status][obligation][sneak]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);
    REQUIRE(game.Owner(2, 4) == game.pA->GetFactionId());
    Unit& rAggressorGuest = game.MakeUnit(*game.pC, 2, 4);

    ApplyHostileAct(*game.pState, game.pC->GetFactionId(), game.pB->GetFactionId());

    const std::vector<PactObligationInteraction_t> obligations =
        game.Queued<PactObligationInteraction_t>();
    REQUIRE(obligations.size() == 1);
    CHECK(obligations.front().kind == VendettaKind_t::SneakAttack);

    HonorDefensiveObligation(*game.pState, game.pA->GetFactionId(), game.pC->GetFactionId(),
                             obligations.front().kind);

    CHECK(game.Status(*game.pA, *game.pC) == DiplomaticStatus_t::Vendetta);
    CHECK(&rAggressorGuest.GetTile() == &game.At(2, 4));
}

TEST_CASE("Factions without a defensive obligation stay out of it",
          "[diplomacy][status][obligation]")
{
    DiplomacyFixture game;
    game.Set(*game.pB, *game.pC, DiplomaticStatus_t::Treaty);

    ApplyHostileAct(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Neutral);
}

TEST_CASE("The player is asked once to honor a Pact when an ally is attacked",
          "[diplomacy][status][obligation]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);

    ApplyHostileAct(*game.pState, game.pC->GetFactionId(), game.pB->GetFactionId());
    ApplyHostileAct(*game.pState, game.pC->GetFactionId(), game.pB->GetFactionId());

    const std::vector<PactObligationInteraction_t> obligations = game.Queued<PactObligationInteraction_t>();
    REQUIRE(obligations.size() == 1);
    CHECK(obligations.front().allyId == game.pB->GetFactionId());
    CHECK(obligations.front().aggressorId == game.pC->GetFactionId());
    CHECK(game.Status(*game.pA, *game.pC) == DiplomaticStatus_t::Neutral);
}

TEST_CASE("Honoring a Pact declares Vendetta on the aggressor", "[diplomacy][status][obligation]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);

    HonorDefensiveObligation(*game.pState, game.pA->GetFactionId(), game.pC->GetFactionId(),
                             VendettaKind_t::Declaration);

    CHECK(game.Status(*game.pA, *game.pC) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Pact);
}

TEST_CASE("Declining a Pact obligation steps the Pact down to a Treaty",
          "[diplomacy][status][obligation]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);

    DeclineDefensiveObligation(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Treaty);
    CHECK(game.Status(*game.pA, *game.pC) == DiplomaticStatus_t::Neutral);
}

TEST_CASE("Native life has no diplomacy to change", "[diplomacy][status][hostile]")
{
    DiplomacyFixture game;
    Faction& rPlanet = AddNativeLifeFaction(game.fixtures, *game.pState);

    ApplyHostileAct(*game.pState, rPlanet.GetFactionId(), game.pA->GetFactionId());
    ApplyHostileAct(*game.pState, game.pA->GetFactionId(), rPlanet.GetFactionId());
    DeclareVendetta(*game.pState, game.pA->GetFactionId(), rPlanet.GetFactionId());
    JoinVendetta(*game.pState, game.pB->GetFactionId(), rPlanet.GetFactionId());
    JoinVendetta(*game.pState, rPlanet.GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pA, rPlanet) == DiplomaticStatus_t::Neutral);
    CHECK(game.Status(*game.pB, rPlanet) == DiplomaticStatus_t::Neutral);
    CHECK_FALSE(game.Ledger().AreKnown(game.pA->GetFactionId(), rPlanet.GetFactionId()));
}

TEST_CASE("Declaring Vendetta by proposal obliges the target's Pact partners",
          "[diplomacy][status][obligation]")
{
    DiplomacyFixture game;
    game.Set(*game.pB, *game.pC, DiplomaticStatus_t::Pact);
    game.Ledger().SetKnown(game.pA->GetFactionId(), game.pB->GetFactionId());

    DiplomaticProposal_t proposal;
    proposal.proposer = game.pA->GetFactionId();
    proposal.recipient = game.pB->GetFactionId();
    proposal.requestedStatus = DiplomaticStatus_t::Vendetta;
    REQUIRE(game.pState->GetDiplomaticActionExecutor().Propose(*game.pState, proposal)
            == DiplomaticProposeResult_t::Accepted);

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Vendetta);
}

TEST_CASE("Attacking a faction already at Vendetta obliges nobody",
          "[diplomacy][status][obligation]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);
    game.Ledger().SetStatus(game.pB->GetFactionId(), game.pC->GetFactionId(),
                            DiplomaticStatus_t::Pact);

    ApplyHostileAct(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Neutral);
}

TEST_CASE("Honoring an obligation as a defender obliges nobody on the aggressor's side",
          "[diplomacy][status][obligation]")
{
    DiplomacyFixture game;
    // D is the aggressor; A, the player, is D's Pact partner; C honors against D.
    Faction& rD = AddSessionFaction(game.fixtures, *game.pState, game.fixtures.factionDefinition,
                                    false);
    game.Set(*game.pA, rD, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, rD, DiplomaticStatus_t::Pact);

    HonorDefensiveObligation(*game.pState, game.pC->GetFactionId(), rD.GetFactionId(),
                             VendettaKind_t::Declaration);

    CHECK(game.Status(*game.pC, rD) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Queued<PactObligationInteraction_t>().empty());
}

TEST_CASE("Honoring an obligation as a separate declaration obliges the aggressor's partners",
          "[diplomacy][status][obligation]")
{
    DiplomacyFixture game;
    Faction& rD = AddSessionFaction(game.fixtures, *game.pState, game.fixtures.factionDefinition,
                                    false);
    game.Set(*game.pA, rD, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, rD, DiplomaticStatus_t::Pact);

    game.fixtures.dataContext.diplomacyConfig->defensiveObligationMode =
        DefensiveObligationMode_t::SeparateDeclaration;

    HonorDefensiveObligation(*game.pState, game.pC->GetFactionId(), rD.GetFactionId(),
                             VendettaKind_t::Declaration);

    CHECK(game.Status(*game.pC, rD) == DiplomaticStatus_t::Vendetta);
    const std::vector<PactObligationInteraction_t> obligations =
        game.Queued<PactObligationInteraction_t>();
    REQUIRE(obligations.size() == 1);
    CHECK(obligations.front().allyId == rD.GetFactionId());
    CHECK(obligations.front().aggressorId == game.pC->GetFactionId());
}

TEST_CASE("Joining a Vendetta already under way changes nothing", "[diplomacy][status][obligation]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);
    ExpireDiplomaticStatuses(*game.pState);
    JoinVendetta(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId());
    CHECK(game.Ledger().GetTurnsHeld(game.pA->GetFactionId(), game.pB->GetFactionId()) == 1);
}

TEST_CASE("Declining an obligation the partner does not owe throws",
          "[diplomacy][status][obligation]")
{
    DiplomacyFixture game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);

    CHECK_THROWS_AS(DeclineDefensiveObligation(*game.pState, game.pA->GetFactionId(),
                                               game.pB->GetFactionId()),
                    std::logic_error);
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("A status change naming a faction outside the session throws and changes nothing",
          "[diplomacy][status]")
{
    DiplomacyFixture game;
    const FactionId_t stranger = game.pState->AllocateFactionId();

    CHECK_THROWS_AS(ApplyStatusChange(*game.pState, game.pA->GetFactionId(), stranger,
                                      DiplomaticStatus_t::Treaty),
                    std::invalid_argument);
    CHECK(game.Ledger().GetStatus(game.pA->GetFactionId(), stranger) == DiplomaticStatus_t::Neutral);
}

#include "GameFixtures.h"

#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/EffectEnums.h"
#include "game/PlayerInteractionQueue.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/DiplomaticActionExecutor.h"
#include "game/faction/TradeItem.h"
#include "game/faction/DiplomacyRules.h"
#include "game/faction/DiplomacyStatusEffects.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/EvacuateTerritoryEffects.h"
#include "game/units/MovementRules.h"
#include "game/units/Pathfinder.h"
#include "game/units/StepEvaluator.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

// Player A (base at 1,4), AI B (base at 7,4), AI C (no base) on an all-land 9x9 map.
struct StatusGame_
{
    FactionFixture fixtures;
    std::unique_ptr<GameState> pState;
    Faction* pA = nullptr;
    Faction* pB = nullptr;
    Faction* pC = nullptr;
    BaseManager* pBaseA = nullptr;
    BaseManager* pBaseB = nullptr;

    StatusGame_()
        : pState(MakeLandSession(fixtures))
    {
        pA = &AddSessionFaction(fixtures, *pState, fixtures.factionDefinition, true);
        pB = &AddSessionFaction(fixtures, *pState, fixtures.factionDefinition, false);
        pC = &AddSessionFaction(fixtures, *pState, fixtures.factionDefinition, false);
        pBaseA = &MakeSessionBase(fixtures, *pState, *pA, 1, 4);
        pBaseB = &MakeSessionBase(fixtures, *pState, *pB, 7, 4);
    }

    DiplomacyLedger& Ledger() { return pState->GetDiplomacyLedger(); }
    WorldMap& Map() { return pState->GetWorldMap(); }
    Tile& At(int x, int y) { return *Map().GetTile(x, y); }
    FactionId_t Owner(int x, int y) { return Map().GetTerritory().GetOwner(x, y); }

    void Set(Faction& rX, Faction& rY, DiplomaticStatus_t status)
    {
        ApplyStatusChange(*pState, rX.GetFactionId(), rY.GetFactionId(), status);
    }

    DiplomaticStatus_t Status(Faction& rX, Faction& rY)
    {
        return Ledger().GetStatus(rX.GetFactionId(), rY.GetFactionId());
    }

    Unit& MakeUnit(Faction& rFaction, int x, int y,
                   const std::vector<std::string>& rComponentIds = {"test_chassis"})
    {
        return MakeSessionUnit(fixtures, *pState, rFaction, x, y, rComponentIds);
    }

    void ProposeVendetta(Faction& rProposer, Faction& rRecipient)
    {
        Ledger().SetKnown(rProposer.GetFactionId(), rRecipient.GetFactionId());
        DiplomaticProposal_t proposal;
        proposal.proposer = rProposer.GetFactionId();
        proposal.recipient = rRecipient.GetFactionId();
        proposal.requestedStatus = DiplomaticStatus_t::Vendetta;
        REQUIRE(pState->GetDiplomaticActionExecutor().Propose(*pState, proposal)
                == DiplomaticProposeResult_t::Accepted);
    }

    bool SharesAnyTile(Faction& rX, Faction& rY)
    {
        for (const Unit& rUnitX : rX.GetUnitManager().Units())
        {
            for (const Unit& rUnitY : rY.GetUnitManager().Units())
            {
                if (&rUnitX.GetTile() == &rUnitY.GetTile())
                {
                    return true;
                }
            }
        }
        return false;
    }

    const InteractionGridsConfig_t& Grids() { return fixtures.dataContext.interactionGrids; }

    template <typename Interaction_t>
    std::vector<Interaction_t> Queued()
    {
        std::vector<Interaction_t> found;
        pState->GetPlayerInteractions().AnyOf([&](const QueuedInteraction_t& rQueued)
        {
            if (const auto* pFound = std::get_if<Interaction_t>(&rQueued.payload))
            {
                found.push_back(*pFound);
            }
            return false;
        });
        return found;
    }
};

} // namespace

TEST_CASE("Only Treaty keeps a faction's units out of the other's territory",
          "[diplomacy][status][movement]")
{
    StatusGame_ game;
    REQUIRE(game.Owner(6, 4) == game.pB->GetFactionId());
    Unit& rScout = game.MakeUnit(*game.pA, 4, 4);

    for (const DiplomaticStatus_t status :
         {DiplomaticStatus_t::Neutral, DiplomaticStatus_t::Pact, DiplomaticStatus_t::Vendetta})
    {
        game.Ledger().SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(), status);
        CHECK(CanEnterTile(rScout, game.At(6, 4), game.Map(), game.Grids()));
    }

    game.Ledger().SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(),
                            DiplomaticStatus_t::Treaty);
    CHECK_FALSE(CanEnterTile(rScout, game.At(6, 4), game.Map(), game.Grids()));
    CHECK(CanEnterTile(rScout, game.At(2, 4), game.Map(), game.Grids()));
}

TEST_CASE("Only Pact partners may stand on the same tile", "[diplomacy][status][movement]")
{
    StatusGame_ game;
    Unit& rMover = game.MakeUnit(*game.pA, 3, 0);
    Unit& rOther = game.MakeUnit(*game.pB, 4, 0);

    CHECK_FALSE(HasFriendlyOccupant(rMover, rOther.GetTile(), game.Map()));
    MoveOrder_t order;
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryStep(rMover, rOther.GetTile(), order).bEntered);

    game.Ledger().SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(),
                            DiplomaticStatus_t::Pact);
    CHECK(HasFriendlyOccupant(rMover, rOther.GetTile(), game.Map()));
    CHECK(HasFriendlyBase(rMover, game.pBaseB->GetTile(), game.Map()));
    CHECK(game.pState->GetUnitOrderExecutor().TryStep(rMover, rOther.GetTile(), order).bEntered);
    CHECK(&rMover.GetTile() == &rOther.GetTile());
}

TEST_CASE("Units repair in their own bases and in Pact partners' bases",
          "[diplomacy][status][repair]")
{
    StatusGame_ game;
    Unit& rUnit = game.MakeUnit(*game.pA, 4, 4);

    CHECK(MayRepairAt(rUnit, *game.pBaseA));
    CHECK_FALSE(MayRepairAt(rUnit, *game.pBaseB));

    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    CHECK_FALSE(MayRepairAt(rUnit, *game.pBaseB));

    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);
    CHECK(MayRepairAt(rUnit, *game.pBaseB));
}

TEST_CASE("A Truce steps down to Neutral once its duration has passed",
          "[diplomacy][status][expiry]")
{
    StatusGame_ game;
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
    StatusGame_ game;
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
    StatusGame_ game;
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
    StatusGame_ game;
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
    StatusGame_ game;
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
    StatusGame_ game;
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
    StatusGame_ game;
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
    StatusGame_ game;
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
    StatusGame_ game;
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
    StatusGame_ game;
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
    StatusGame_ game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    Unit& rAttacker = game.MakeUnit(*game.pA, 4, 0, {"test_chassis", "test_weapon"});
    Unit& rDefender = game.MakeUnit(*game.pB, 5, 0);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryAttack(rAttacker, rDefender.GetTile()));
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
}

TEST_CASE("Bombarding a faction declares Vendetta", "[diplomacy][status][hostile]")
{
    StatusGame_ game;
    Unit& rAttacker = game.MakeUnit(*game.pA, 4, 0, {"test_chassis", "bombard"});
    game.MakeUnit(*game.pB, 5, 0);

    REQUIRE(game.pState->GetUnitOrderExecutor().TryBombard(rAttacker, game.At(5, 0)));
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
}

TEST_CASE("An AI Pact partner of the victim declares Vendetta on the aggressor",
          "[diplomacy][status][obligation]")
{
    StatusGame_ game;
    game.Set(*game.pB, *game.pC, DiplomaticStatus_t::Pact);

    ApplyHostileAct(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Status(*game.pB, *game.pC) == DiplomaticStatus_t::Pact);
}

TEST_CASE("A Pact partner defending against a declaration leaves the aggressor's territory",
          "[diplomacy][status][obligation][declaration]")
{
    StatusGame_ game;
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
    StatusGame_ game;
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
    StatusGame_ game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);
    REQUIRE(game.Owner(2, 4) == game.pA->GetFactionId());
    Unit& rAggressorGuest = game.MakeUnit(*game.pC, 2, 4);

    ApplyHostileAct(*game.pState, game.pC->GetFactionId(), game.pB->GetFactionId());

    const std::vector<PactObligationInteraction_t> obligations =
        game.Queued<PactObligationInteraction_t>();
    REQUIRE(obligations.size() == 1);
    CHECK(obligations.front().entry == VendettaEntry_t::SneakAttack);

    ResolveDefensiveObligation(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId(),
                               game.pC->GetFactionId(), true, obligations.front().entry);

    CHECK(game.Status(*game.pA, *game.pC) == DiplomaticStatus_t::Vendetta);
    CHECK(&rAggressorGuest.GetTile() == &game.At(2, 4));
}

TEST_CASE("Factions without a defensive obligation stay out of it",
          "[diplomacy][status][obligation]")
{
    StatusGame_ game;
    game.Set(*game.pB, *game.pC, DiplomaticStatus_t::Treaty);

    ApplyHostileAct(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Neutral);
}

TEST_CASE("The player is asked once to honor a Pact when an ally is attacked",
          "[diplomacy][status][obligation]")
{
    StatusGame_ game;
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
    StatusGame_ game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);

    ResolveDefensiveObligation(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId(),
                               game.pC->GetFactionId(), true, VendettaEntry_t::Declaration);

    CHECK(game.Status(*game.pA, *game.pC) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Pact);
}

TEST_CASE("Declining a Pact obligation steps the Pact down to a Treaty",
          "[diplomacy][status][obligation]")
{
    StatusGame_ game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Pact);

    ResolveDefensiveObligation(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId(),
                               game.pC->GetFactionId(), false, VendettaEntry_t::Declaration);

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Treaty);
    CHECK(game.Status(*game.pA, *game.pC) == DiplomaticStatus_t::Neutral);
}

TEST_CASE("Native life has no diplomacy to change", "[diplomacy][status][hostile]")
{
    StatusGame_ game;
    Faction& rPlanet = AddNativeLifeFaction(game.fixtures, *game.pState);

    ApplyHostileAct(*game.pState, rPlanet.GetFactionId(), game.pA->GetFactionId());
    ApplyHostileAct(*game.pState, game.pA->GetFactionId(), rPlanet.GetFactionId());

    CHECK(game.Status(*game.pA, rPlanet) == DiplomaticStatus_t::Neutral);
}

TEST_CASE("A faction's commerce rate follows its status with each partner",
          "[diplomacy][status][commerce]")
{
    StatusGame_ game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, *game.pC, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, *game.pC, DiplomaticStatus_t::Pact);

    const auto rateToward = [&](const Faction* pPartner)
    {
        EffectContext_t ctx;
        ctx.pFaction = game.pA;
        ctx.pPartner = pPartner;
        return ResolveFactionStat(game.pA->GetActiveEffects(), StatId_t::CommerceRate, 1.0, &ctx);
    };

    CHECK(rateToward(game.pB) == 0.5);
    CHECK(rateToward(game.pC) == 1.0);
    // Pair effects never reach a resolve that names no partner.
    CHECK(rateToward(nullptr) == 1.0);
    CHECK(ResolveFactionStat(game.pA->GetActiveEffects(), StatId_t::CommerceRate, 1.0) == 1.0);

    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);
    CHECK(rateToward(game.pB) == 0.0);

    Faction& rLate = AddSessionFaction(game.fixtures, *game.pState,
                                       game.fixtures.factionDefinition, false);
    CHECK(rateToward(&rLate) == 0.0);
}

TEST_CASE("FactionPair scope belongs to diplomatic statuses alone", "[diplomacy][status][effects]")
{
    const nlohmann::json container = nlohmann::json::parse(R"({
      "effects": [{ "type": "StatModifier", "scope": "FactionPair",
                    "parameters": { "stat": "commerce_rate", "amount": 2, "op": "MultiplyGeometric" } }]
    })");
    CHECK_THROWS_AS(
        EffectConfigParser::ParseEffects(container, EffectSourceKind_t::Building, "test_building"),
        std::runtime_error);
    CHECK_NOTHROW(EffectConfigParser::ParseEffects(container, EffectSourceKind_t::DiplomaticStatus,
                                                   "statuses.Treaty"));
}


TEST_CASE("A player's move stops at the border of Treaty territory and asks",
          "[diplomacy][status][territory]")
{
    StatusGame_ game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    REQUIRE(game.Owner(4, 4) == game.pA->GetFactionId());
    REQUIRE(game.Owner(5, 4) == game.pB->GetFactionId());
    Unit& rScout = game.MakeUnit(*game.pA, 4, 4);

    // Planning routes into the territory; only the step itself is refused.
    REQUIRE(game.pState->GetPathfinder().FindPath(rScout, game.At(6, 4)).bReachable);
    const StepEvaluator steps(game.Map(), game.pState->GetTileEffects());
    CHECK(steps.EvaluateStep(rScout, rScout.GetTile(), game.At(5, 4)).outcome
          == StepOutcome_t::BlockedByTerritory);

    rScout.SetOrder(MoveOrder_t{&game.At(6, 4)});
    UnitOrderExecutor& rOrders = game.pState->GetUnitOrderExecutor();
    CHECK(rOrders.Execute(rScout) == OrderProgress_t::Continue);

    CHECK(&rScout.GetTile() == &game.At(4, 4));
    CHECK(rScout.GetOrder().has_value());
    const std::vector<TerritoryEntryInteraction_t> entries = game.Queued<TerritoryEntryInteraction_t>();
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().unitId == rScout.GetUnitId());
    CHECK(entries.front().ownerId == game.pB->GetFactionId());
}

TEST_CASE("Breaking the Treaty at the border declares Vendetta and resumes the move",
          "[diplomacy][status][territory]")
{
    StatusGame_ game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    Unit& rScout = game.MakeUnit(*game.pA, 4, 4);
    rScout.SetOrder(MoveOrder_t{&game.At(5, 4)});
    REQUIRE(game.pState->GetUnitOrderExecutor().Execute(rScout) == OrderProgress_t::Continue);

    ResolveTerritoryEntry(*game.pState, rScout, game.pB->GetFactionId(), true);

    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Vendetta);
    CHECK(&rScout.GetTile() == &game.At(5, 4));
}

TEST_CASE("Cancelling at the border drops the order and keeps the Treaty",
          "[diplomacy][status][territory]")
{
    StatusGame_ game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    Unit& rScout = game.MakeUnit(*game.pA, 4, 4);
    rScout.SetOrder(MoveOrder_t{&game.At(5, 4)});
    REQUIRE(game.pState->GetUnitOrderExecutor().Execute(rScout) == OrderProgress_t::Continue);

    ResolveTerritoryEntry(*game.pState, rScout, game.pB->GetFactionId(), false);

    CHECK_FALSE(rScout.GetOrder().has_value());
    CHECK(&rScout.GetTile() == &game.At(4, 4));
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("An AI unit drops a move order that would enter forbidden territory",
          "[diplomacy][status][territory]")
{
    StatusGame_ game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    Unit& rScout = game.MakeUnit(*game.pB, 5, 4);
    rScout.SetOrder(MoveOrder_t{&game.At(3, 4)});

    CHECK(game.pState->GetUnitOrderExecutor().Execute(rScout) == OrderProgress_t::Complete);
    CHECK_FALSE(rScout.GetOrder().has_value());
    CHECK(&rScout.GetTile() == &game.At(5, 4));
    CHECK(game.Queued<TerritoryEntryInteraction_t>().empty());
    CHECK(game.Status(*game.pA, *game.pB) == DiplomaticStatus_t::Treaty);
}

TEST_CASE("Path planning keeps out of forbidden territory when a route avoids it",
          "[diplomacy][status][territory]")
{
    StatusGame_ game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    Unit& rScout = game.MakeUnit(*game.pA, 4, 0);

    const Path_t path = game.pState->GetPathfinder().FindPath(rScout, game.At(4, 8));
    REQUIRE(path.bReachable);
    for (const Tile* pTile : path.tiles)
    {
        CHECK(game.Map().GetTerritory().GetOwner(*pTile) != game.pB->GetFactionId());
    }
}

TEST_CASE("Declaring Vendetta by proposal obliges the target's Pact partners",
          "[diplomacy][status][obligation]")
{
    StatusGame_ game;
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
    StatusGame_ game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);
    game.Ledger().SetStatus(game.pB->GetFactionId(), game.pC->GetFactionId(),
                            DiplomaticStatus_t::Pact);

    ApplyHostileAct(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId());

    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Neutral);
}

TEST_CASE("Breaking a Treaty at the border obliges the owner's Pact partners",
          "[diplomacy][status][obligation]")
{
    StatusGame_ game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Treaty);
    game.Set(*game.pB, *game.pC, DiplomaticStatus_t::Pact);
    Unit& rScout = game.MakeUnit(*game.pA, 4, 4);
    rScout.SetOrder(MoveOrder_t{&game.At(5, 4)});
    REQUIRE(game.pState->GetUnitOrderExecutor().Execute(rScout) == OrderProgress_t::Continue);

    ResolveTerritoryEntry(*game.pState, rScout, game.pB->GetFactionId(), true);

    CHECK(game.Status(*game.pC, *game.pA) == DiplomaticStatus_t::Vendetta);
}

TEST_CASE("Honoring an obligation as a defender obliges nobody on the aggressor's side",
          "[diplomacy][status][obligation]")
{
    StatusGame_ game;
    // D is the aggressor; A, the player, is D's Pact partner; C honors against D.
    Faction& rD = AddSessionFaction(game.fixtures, *game.pState, game.fixtures.factionDefinition,
                                    false);
    game.Set(*game.pA, rD, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, rD, DiplomaticStatus_t::Pact);

    HonorDefensiveObligation(*game.pState, game.pC->GetFactionId(), rD.GetFactionId(),
                             DefensiveObligationMode_t::JoinAsDefender,
                             VendettaEntry_t::Declaration);

    CHECK(game.Status(*game.pC, rD) == DiplomaticStatus_t::Vendetta);
    CHECK(game.Queued<PactObligationInteraction_t>().empty());
}

TEST_CASE("Honoring an obligation as a separate declaration obliges the aggressor's partners",
          "[diplomacy][status][obligation]")
{
    StatusGame_ game;
    Faction& rD = AddSessionFaction(game.fixtures, *game.pState, game.fixtures.factionDefinition,
                                    false);
    game.Set(*game.pA, rD, DiplomaticStatus_t::Treaty);
    game.Set(*game.pA, rD, DiplomaticStatus_t::Pact);

    HonorDefensiveObligation(*game.pState, game.pC->GetFactionId(), rD.GetFactionId(),
                             DefensiveObligationMode_t::SeparateDeclaration,
                             VendettaEntry_t::Declaration);

    CHECK(game.Status(*game.pC, rD) == DiplomaticStatus_t::Vendetta);
    const std::vector<PactObligationInteraction_t> obligations =
        game.Queued<PactObligationInteraction_t>();
    REQUIRE(obligations.size() == 1);
    CHECK(obligations.front().allyId == rD.GetFactionId());
    CHECK(obligations.front().aggressorId == game.pC->GetFactionId());
}

TEST_CASE("Joining a Vendetta already under way changes nothing", "[diplomacy][status][obligation]")
{
    StatusGame_ game;
    game.Set(*game.pA, *game.pB, DiplomaticStatus_t::Vendetta);
    ExpireDiplomaticStatuses(*game.pState);
    JoinVendetta(*game.pState, game.pA->GetFactionId(), game.pB->GetFactionId());
    CHECK(game.Ledger().GetTurnsHeld(game.pA->GetFactionId(), game.pB->GetFactionId()) == 1);
}

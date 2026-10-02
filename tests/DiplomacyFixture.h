#pragma once

#include "GameFixtures.h"

#include "game/GameState.h"
#include "game/PlayerInteractionQueue.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/DiplomaticActionExecutor.h"
#include "game/faction/DiplomaticTransitionEffects.h"
#include "game/faction/TradeItem.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace actest
{

// Player A (base at 1,4), AI B (base at 7,4), AI C (no base) on an all-land 9x9 map.
struct DiplomacyFixture
{
    FactionFixture fixtures;
    std::unique_ptr<ac::GameState> pState;
    ac::Faction* pA = nullptr;
    ac::Faction* pB = nullptr;
    ac::Faction* pC = nullptr;
    ac::BaseManager* pBaseA = nullptr;
    ac::BaseManager* pBaseB = nullptr;

    DiplomacyFixture()
        : pState(MakeLandSession(fixtures))
    {
        pA = &AddSessionFaction(fixtures, *pState, fixtures.factionDefinition, true);
        pB = &AddSessionFaction(fixtures, *pState, fixtures.factionDefinition, false);
        pC = &AddSessionFaction(fixtures, *pState, fixtures.factionDefinition, false);
        pBaseA = &MakeSessionBase(fixtures, *pState, *pA, 1, 4);
        pBaseB = &MakeSessionBase(fixtures, *pState, *pB, 7, 4);
    }

    ac::DiplomacyLedger& Ledger() { return pState->GetDiplomacyLedger(); }
    ac::WorldMap& Map() { return pState->GetWorldMap(); }
    ac::Tile& At(int x, int y) { return *Map().GetTile(x, y); }
    ac::FactionId_t Owner(int x, int y) { return Map().GetTerritory().GetOwner(x, y); }

    void Set(ac::Faction& rX, ac::Faction& rY, ac::DiplomaticStatus_t status)
    {
        ac::ApplyStatusChange(*pState, rX.GetFactionId(), rY.GetFactionId(), status);
    }

    ac::DiplomaticStatus_t Status(ac::Faction& rX, ac::Faction& rY)
    {
        return Ledger().GetStatus(rX.GetFactionId(), rY.GetFactionId());
    }

    ac::Unit& MakeUnit(ac::Faction& rFaction, int x, int y,
                   const std::vector<std::string>& rComponentIds = {"test_chassis"})
    {
        return MakeSessionUnit(fixtures, *pState, rFaction, x, y, rComponentIds);
    }

    void ProposeVendetta(ac::Faction& rProposer, ac::Faction& rRecipient)
    {
        Ledger().SetKnown(rProposer.GetFactionId(), rRecipient.GetFactionId());
        ac::DiplomaticProposal_t proposal;
        proposal.proposer = rProposer.GetFactionId();
        proposal.recipient = rRecipient.GetFactionId();
        proposal.requestedStatus = ac::DiplomaticStatus_t::Vendetta;
        REQUIRE(pState->GetDiplomaticActionExecutor().Propose(*pState, proposal)
                == ac::DiplomaticProposeResult_t::Accepted);
    }

    bool SharesAnyTile(ac::Faction& rX, ac::Faction& rY)
    {
        for (const ac::Unit& rUnitX : rX.GetUnitManager().Units())
        {
            for (const ac::Unit& rUnitY : rY.GetUnitManager().Units())
            {
                if (&rUnitX.GetTile() == &rUnitY.GetTile())
                {
                    return true;
                }
            }
        }
        return false;
    }

    const ac::InteractionGridsConfig_t& Grids() { return fixtures.dataContext.interactionGrids; }

    template <typename Interaction_t>
    std::vector<Interaction_t> Queued()
    {
        std::vector<Interaction_t> found;
        pState->GetPlayerInteractions().AnyOf([&](const ac::QueuedInteraction_t& rQueued)
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

} // namespace actest

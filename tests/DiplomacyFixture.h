#pragma once

#include "GameFixtures.h"

#include "game/PlayerInteractionQueue.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <variant>
#include <vector>

namespace actest
{

// Player A (base at 5,5), AI B (base at 11,11), AI C (no base) on an all-land map.
struct DiplomacyFixture
{
    FactionFixture fixtures;
    ac::Faction* pA = nullptr;
    ac::Faction* pB = nullptr;
    ac::Faction* pC = nullptr;
    ac::BaseManager* pBaseA = nullptr;
    ac::BaseManager* pBaseB = nullptr;

    DiplomacyFixture()
    {
        FillLand(fixtures.map);
        pA = &AddSessionFaction(fixtures, fixtures.factionDefinition, true);
        pB = &AddSessionFaction(fixtures, fixtures.factionDefinition, false);
        pC = &AddSessionFaction(fixtures, fixtures.factionDefinition, false);
        pBaseA = &MakeSessionBase(fixtures, *pA, 5, 5);
        pBaseB = &MakeSessionBase(fixtures, *pB, 11, 11);
    }

    ac::DiplomacyLedger& Ledger() { return fixtures.state.GetDiplomacyLedger(); }
    ac::WorldMap& Map() { return fixtures.map; }
    ac::Tile& At(int x, int y) { return *Map().GetTile(x, y); }
    ac::FactionId_t Owner(int x, int y) { return Map().GetTerritory().GetOwner(x, y); }

    void Set(ac::Faction& rX, ac::Faction& rY, ac::DiplomaticStatus_t status)
    {
        ac::ApplyStatusChange(fixtures.state, rX.GetFactionId(), rY.GetFactionId(), status);
    }

    ac::DiplomaticStatus_t Status(ac::Faction& rX, ac::Faction& rY)
    {
        return Ledger().GetStatus(rX.GetFactionId(), rY.GetFactionId());
    }

    ac::Unit& MakeUnit(ac::Faction& rFaction, int x, int y,
                   const std::vector<std::string>& rComponentIds = {"test_chassis"})
    {
        return fixtures.MakeUnit(rFaction, x, y, rComponentIds);
    }

    void MeetAll()
    {
        Ledger().SetKnown({pA->GetFactionId(), pB->GetFactionId(), pC->GetFactionId()});
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
        fixtures.state.GetPlayerInteractions().AnyOf([&](const ac::QueuedInteraction_t& rQueued)
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

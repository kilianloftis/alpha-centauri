#include "game/orbital/OrbitalCensus.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ac
{

namespace
{

std::unordered_map<BuildingId_t, int> TallyOrbitalBuildings_(const Faction& rFaction)
{
    std::unordered_map<BuildingId_t, int> counts;
    for (const BaseManager& rBase : rFaction.Bases())
    {
        for (const BuildingConfig_t* pBuilding : rBase.GetBuildingManager().GetBuildings())
        {
            if (pBuilding && pBuilding->orbital)
            {
                ++counts[pBuilding->id];
            }
        }
    }
    return counts;
}

} // namespace

std::vector<OrbitalCensusEntry_t> BuildOrbitalCensus(const GameState& rGameState)
{
    std::vector<OrbitalCensusEntry_t> census;
    for (const Faction& rFaction : rGameState.Factions())
    {
        for (const auto& [buildingId, count] : TallyOrbitalBuildings_(rFaction))
        {
            census.push_back(OrbitalCensusEntry_t{rFaction.GetFactionId(), buildingId, count});
        }
    }
    return census;
}

int CountFactionOrbitalBuildings(const GameState& rGameState,
                                 FactionId_t factionId,
                                 const BuildingId_t& buildingId)
{
    const Faction* pFaction = rGameState.FindFaction(factionId);
    if (!pFaction)
    {
        throw std::runtime_error("CountFactionOrbitalBuildings: unknown faction id "
                                 + std::to_string(factionId));
    }
    // Owned nowhere, or owned but not orbital — both are genuinely zero, not a lookup failure.
    const BuildingConfig_t* pConfig = pFaction->FindOwnedBuildingConfig(buildingId);
    if (!pConfig || !pConfig->orbital)
    {
        return 0;
    }
    return pFaction->CountBuildings(buildingId);
}

} // namespace ac

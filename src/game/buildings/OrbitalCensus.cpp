#include "game/buildings/OrbitalCensus.h"

#include "game/CurrentGameState.h"
#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"

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

std::vector<OrbitalCensusEntry_t> BuildOrbitalCensus()
{
    const GameState& rGameState = CurrentGameState();
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

} // namespace ac

#pragma once

#include "game/buildings/BuildingConfig.h"
#include "game/base/BaseTypes.h"

#include <vector>

namespace ac
{


struct OrbitalCensusEntry_t
{
    FactionId_t factionId = 0;
    BuildingId_t buildingId;
    int count = 0;
};

// Public census of buildings with orbital == true (no fog/intel gate).
std::vector<OrbitalCensusEntry_t> BuildOrbitalCensus();

// Zero when the faction owns no copy, or owns one that is not orbital.
// Throws std::runtime_error when factionId names no faction.
int CountFactionOrbitalBuildings(FactionId_t factionId, const BuildingId_t& buildingId);

} // namespace ac

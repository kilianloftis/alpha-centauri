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

} // namespace ac

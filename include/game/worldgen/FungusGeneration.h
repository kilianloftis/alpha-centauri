#pragma once

#include "game/worldgen/WorldGenDecorationConfig.h"

#include <random>

namespace ac
{

class WorldMap;
struct ImprovementConfig_t;

// Place xenofungus from coverage / cluster scale / fill, scaled by native-life multiplier.
void PlaceFungus(WorldMap& rWorld, const FungusDecorationConfig_t& rConfig,
                 float coverageMultiplier, const ImprovementConfig_t& rFungus,
                 std::mt19937& rRng);

} // namespace ac

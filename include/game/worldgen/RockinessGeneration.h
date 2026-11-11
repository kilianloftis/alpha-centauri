#pragma once

#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/worldgen/WorldGenDecorationConfig.h"

#include <random>

namespace ac
{

// Band whose half-open elevation range owns elevationAboveOcean (last band is unbounded above).
const RockinessBand_t& BandForElevation(const RockinessDecorationConfig_t& rConfig,
                                        int elevationAboveOcean);

// u in [0, 1). Weights are expected non-negative after parse normalization; zero-sum throws.
Rockiness_t SampleRockiness(const RockinessWeights_t& rWeights, float u);

// Staggered scan: pole rows sample poles weights; others sample BandForElevation.
void PlaceRockiness(WorldMap& rWorld, const RockinessDecorationConfig_t& rConfig,
                    std::mt19937& rRng);

} // namespace ac

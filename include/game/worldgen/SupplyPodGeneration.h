#pragma once

#include "game/supply/SupplyPodConfig.h"

#include <random>

namespace ac
{

class ImprovementRegistry;
class SupplyPodIndex;
class Tile;
class WorldMap;

// Global scatter for every event group's placement.global_fraction of eligible tiles.
int PlaceGlobalSupplyPods(WorldMap& rWorld, const SupplyPodConfig_t& rConfig,
                          const ImprovementRegistry& rImprovementRegistry, SupplyPodIndex& rIndex,
                          std::mt19937& rRng);

// Extra pods near Unity wreckage for every event group's wreckage knobs.
int PlaceWreckageSupplyPods(WorldMap& rWorld, const SupplyPodConfig_t& rConfig,
                            const ImprovementRegistry& rImprovementRegistry,
                            SupplyPodIndex& rIndex, std::mt19937& rRng);

// Start-ring pods for every event group's start_radius / start_count.
int PlaceStartSupplyPods(WorldMap& rWorld, const Tile& rStart, const SupplyPodConfig_t& rConfig,
                         const ImprovementRegistry& rImprovementRegistry, SupplyPodIndex& rIndex,
                         std::mt19937& rRng);

} // namespace ac

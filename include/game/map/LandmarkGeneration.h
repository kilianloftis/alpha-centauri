#pragma once

#include "game/map/LandmarkConfig.h"

#include <random>
#include <utility>
#include <vector>

namespace ac
{

class WorldMap;
class ImprovementRegistry;
class Tile;

// Relative (dx, dy) offsets for a landmark shape, centered on the anchor.
std::vector<std::pair<int, int>> ExpandLandmarkShape(const LandmarkShape_t& rShape);

// Place landmarks on rWorld. Stamps improvementId on each footprint tile that matches
// domain and CanBuildImprovement. Returns number of landmarks successfully placed.
int PlaceLandmarks(WorldMap& rWorld,
                   const std::vector<LandmarkConfig_t>& rLandmarks,
                   const ImprovementRegistry& rImprovements,
                   std::mt19937& rRng);

} // namespace ac

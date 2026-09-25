#pragma once

#include "game/map/ElevationRulesConfig.h"

#include <random>

namespace ac
{

class Tile;
class WorldMap;

// One uniform draw in [level_min_meters, level_max_meters].
int RollLevelMeters(std::mt19937& rRng, const ElevationRulesConfig_t& rRules);

// Add deltaMeters to rOrigin, clamp to [floorMeters, ceilingMeters] and to Planet's
// elevation range, then pull Chebyshev neighbors of every tile this call changed until
// each such pair differs by at most max_adjacent_difference_meters. Neighbors clamp only
// to Planet's range. Returns false when the origin elevation does not change.
// Recomputes rivers when any tile changed.
bool ApplyElevationDelta(Tile& rOrigin, WorldMap& rWorldMap, int deltaMeters,
                         const ElevationRulesConfig_t& rRules, int floorMeters, int ceilingMeters);

// Raise rOrigin by levelCount independent level rolls, clamped to Planet's elevation range.
// levelCount <= 0 changes nothing. Not invoked from combat.
bool ApplyEarthquake(Tile& rOrigin, WorldMap& rWorldMap, int levelCount, std::mt19937& rRng,
                     const ElevationRulesConfig_t& rRules);

} // namespace ac

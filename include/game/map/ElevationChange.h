#pragma once

#include "game/map/ElevationRulesConfig.h"

#include <random>

namespace ac
{

class IUnitOrderWorld;
class Tile;
class TileEffectsContext;
class WorldMap;

// A tile whose elevation this call crossed ocean level. bNowWater is the surface it landed on.
struct SurfaceFlip_t
{
    Tile* pTile = nullptr;
    bool bNowWater = false;
};

// One uniform draw in [level_min_meters, level_max_meters].
int RollLevelMeters(std::mt19937& rRng, const ElevationRulesConfig_t& rRules);

// Add deltaMeters to rOrigin, clamp to [floorMeters, ceilingMeters] and to Planet's
// elevation range, then pull Chebyshev neighbors of every tile this call changed until
// each such pair differs by at most max_adjacent_difference_meters. Neighbors clamp only
// to Planet's range. Returns false when the origin elevation does not change.
// Recomputes rivers when any tile changed.
// When pTileEffects is set, tiles that crossed ocean level lose improvements whose domain
// no longer matches, and a base that cannot occupy water is razed.
bool ApplyElevationDelta(Tile& rOrigin, WorldMap& rWorldMap, int deltaMeters,
                         const ElevationRulesConfig_t& rRules, int floorMeters, int ceilingMeters,
                         TileEffectsContext* pTileEffects = nullptr,
                         IUnitOrderWorld* pWorld = nullptr);

// Raise rOrigin by levelCount independent level rolls, clamped to Planet's elevation range.
// levelCount <= 0 changes nothing. Not invoked from combat. Occupancy matches ApplyElevationDelta.
bool ApplyEarthquake(Tile& rOrigin, WorldMap& rWorldMap, int levelCount, std::mt19937& rRng,
                     const ElevationRulesConfig_t& rRules,
                     TileEffectsContext* pTileEffects = nullptr,
                     IUnitOrderWorld* pWorld = nullptr);

} // namespace ac

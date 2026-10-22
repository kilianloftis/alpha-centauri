#pragma once

#include "game/map/ElevationRulesConfig.h"

#include <random>
#include <span>

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

// Adds deltaMeters to rOrigin within the floor, ceiling and Planet's range, then relaxes the
// slope around every changed tile. False when the origin does not change.
bool ApplyElevationDelta(Tile& rOrigin, WorldMap& rWorldMap, int deltaMeters,
                         const ElevationRulesConfig_t& rRules, int floorMeters, int ceilingMeters,
                         TileEffectsContext* pTileEffects,
                         IUnitOrderWorld* pWorld);

// Raise rOrigin by levelCount independent level rolls, clamped to Planet's elevation range.
// levelCount <= 0 changes nothing. Not invoked from combat. Occupancy matches ApplyElevationDelta.
bool ApplyEarthquake(Tile& rOrigin, WorldMap& rWorldMap, int levelCount, std::mt19937& rRng,
                     const ElevationRulesConfig_t& rRules,
                     TileEffectsContext* pTileEffects,
                     IUnitOrderWorld* pWorld);

// Drops each tile one rolled level, then relaxes the slope once, so no neighbour is pulled
// before its own roll. False when nothing changes.
bool LowerTilesOneLevel(std::span<Tile*> tiles, WorldMap& rWorldMap, std::mt19937& rRng,
                        const ElevationRulesConfig_t& rRules,
                        TileEffectsContext* pTileEffects,
                        IUnitOrderWorld* pWorld);

} // namespace ac

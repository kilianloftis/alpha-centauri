#pragma once

#include "game/map/ElevationRulesConfig.h"
#include "game/map/Tile.h"

#include <optional>
#include <random>
#include <span>
#include <vector>

namespace ac
{

class GameState;

// A tile whose elevation this call crossed ocean level. bNowWater is the surface it landed on.
struct SurfaceFlip_t
{
    Tile* pTile = nullptr;
    bool bNowWater = false;
};

// One uniform draw in [level_min_meters, level_max_meters].
int RollLevelMeters(std::mt19937& rRng, const ElevationRulesConfig_t& rRules);

// Adds deltaMeters to rOrigin within the floor, ceiling and Planet's range, then relaxes the
// slope around every changed tile. The climate rerun's base-moisture changes, or nullopt when the
// origin does not change.
std::optional<std::vector<MoistureChange_t>> ApplyElevationDelta(
    Tile& rOrigin, int deltaMeters, const ElevationRulesConfig_t& rRules, int floorMeters,
    int ceilingMeters, GameState& rGameState);

// Raise rOrigin by levelCount independent level rolls, clamped to Planet's elevation range.
// levelCount <= 0 changes nothing. Not invoked from combat. Occupancy matches ApplyElevationDelta.
bool ApplyEarthquake(Tile& rOrigin, int levelCount, std::mt19937& rRng,
                     const ElevationRulesConfig_t& rRules, GameState& rGameState);

// Drops each tile one rolled level, then relaxes the slope once, so no neighbour is pulled
// before its own roll. False when nothing changes.
bool LowerTilesOneLevel(std::span<Tile*> tiles, std::mt19937& rRng,
                        const ElevationRulesConfig_t& rRules, GameState& rGameState);

} // namespace ac

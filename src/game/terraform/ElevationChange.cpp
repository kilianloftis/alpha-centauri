#include "game/terraform/ElevationChange.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/terraform/SurfaceOccupancy.h"
#include "game/terraform/TerraformRules.h"

#include <span>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace ac
{

namespace
{

// Each written tile's surface before its first write.
std::vector<SurfaceFlip_t> WriteElevations_(const std::vector<ElevationWrite_t>& rWrites,
                                            WorldMap& rWorldMap)
{
    std::vector<SurfaceFlip_t> before;
    std::unordered_set<const Tile*> seen;
    for (const ElevationWrite_t& rWrite : rWrites)
    {
        Tile& rTile = rWorldMap.MutableTile(*rWrite.pTile);
        if (seen.insert(&rTile).second)
        {
            before.push_back(SurfaceFlip_t{&rTile, rTile.IsWater()});
        }
        rTile.SetElevation(rWrite.elevation);
    }
    return before;
}

std::vector<SurfaceFlip_t> FlippedSurfaces_(const std::vector<SurfaceFlip_t>& rBefore)
{
    std::vector<SurfaceFlip_t> flips;
    for (const SurfaceFlip_t& rTileBefore : rBefore)
    {
        if (rTileBefore.pTile->IsWater() != rTileBefore.bNowWater)
        {
            flips.push_back(SurfaceFlip_t{rTileBefore.pTile, rTileBefore.pTile->IsWater()});
        }
    }
    return flips;
}

// Applies the writes in order, then reconciles the tiles whose surface ended up flipped and
// recomputes the climate. Returns false when there was nothing to write.
bool ApplyElevationWrites_(const std::vector<ElevationWrite_t>& rWrites, GameState& rGameState)
{
    if (rWrites.empty())
    {
        return false;
    }

    TileChangeDeferral defer;
    WorldMap& rWorldMap = rGameState.GetWorldMap();
    const std::vector<SurfaceFlip_t> before = WriteElevations_(rWrites, rWorldMap);
    ReconcileSurfaceFlips(rGameState, FlippedSurfaces_(before));
    rGameState.GetTileEffects().RecomputeClimate();
    return true;
}

} // namespace

int RollLevelMeters(std::mt19937& rRng, const ElevationRulesConfig_t& rRules)
{
    if (rRules.levelMinMeters <= 0 || rRules.levelMaxMeters < rRules.levelMinMeters)
    {
        throw std::logic_error("RollLevelMeters: level meter range is invalid");
    }
    std::uniform_int_distribution<int> distribution(rRules.levelMinMeters, rRules.levelMaxMeters);
    return distribution(rRng);
}

bool ApplyElevationDelta(Tile& rOrigin, int deltaMeters, const ElevationRulesConfig_t& rRules,
                         int floorMeters, int ceilingMeters, GameState& rGameState)
{
    RequireCurrentGameState(rGameState);
    const ElevationSeed_t seed{&rOrigin, deltaMeters};
    const std::vector<ElevationWrite_t> writes = PlanElevationChange(
        std::span<const ElevationSeed_t>(&seed, 1), floorMeters, ceilingMeters, rRules);
    return ApplyElevationWrites_(writes, rGameState);
}

bool ApplyEarthquake(Tile& rOrigin, int levelCount, std::mt19937& rRng,
                     const ElevationRulesConfig_t& rRules, GameState& rGameState)
{
    RequireCurrentGameState(rGameState);
    if (levelCount <= 0)
    {
        return false;
    }

    // Every level is rolled, whatever the sum, so the RNG stream advances by levelCount.
    std::vector<int> rolls;
    rolls.reserve(static_cast<size_t>(levelCount));
    for (int level = 0; level < levelCount; ++level)
    {
        rolls.push_back(RollLevelMeters(rRng, rRules));
    }
    const int delta = EarthquakeDeltaMeters(rolls, rRules);
    return ApplyElevationDelta(rOrigin, delta, rRules, rRules.minElevationMeters,
                               rRules.maxElevationMeters, rGameState);
}

bool LowerTilesOneLevel(std::span<Tile*> tiles, std::mt19937& rRng,
                        const ElevationRulesConfig_t& rRules, GameState& rGameState)
{
    RequireCurrentGameState(rGameState);
    if (tiles.empty())
    {
        return false;
    }

    // Roll even when the tile is already on the floor, so the stream advances once per tile.
    std::vector<ElevationSeed_t> seeds;
    seeds.reserve(tiles.size());
    for (Tile* pTile : tiles)
    {
        if (pTile)
        {
            seeds.push_back(ElevationSeed_t{pTile, -RollLevelMeters(rRng, rRules)});
        }
    }
    const std::vector<ElevationWrite_t> writes = PlanElevationChange(
        seeds, rRules.minElevationMeters, rRules.maxElevationMeters, rRules);
    return ApplyElevationWrites_(writes, rGameState);
}

} // namespace ac

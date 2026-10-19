#include "game/terraform/ElevationChange.h"

#include "game/effects/TileEffectsContext.h"
#include "game/map/RiverGeneration.h"
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

Tile& MutableTile_(WorldMap& rWorldMap, const Tile& rTile)
{
    Tile* pTile = rWorldMap.GetTile(rTile.GetX(), rTile.GetY());
    if (pTile == nullptr)
    {
        throw std::logic_error("ElevationChange: a planned tile is not on this world");
    }
    return *pTile;
}

// Applies the writes in order, then reconciles the tiles whose surface ended up flipped and
// recomputes rivers. Returns false when there was nothing to write.
bool ApplyElevationWrites_(const std::vector<ElevationWrite_t>& rWrites, WorldMap& rWorldMap,
                           TileEffectsContext* pTileEffects, IUnitOrderWorld* pWorld)
{
    if (rWrites.empty())
    {
        return false;
    }

    TileChangeDeferral defer;
    std::vector<SurfaceFlip_t> before;
    std::unordered_set<const Tile*> seen;
    for (const ElevationWrite_t& rWrite : rWrites)
    {
        Tile& rTile = MutableTile_(rWorldMap, *rWrite.pTile);
        if (seen.insert(&rTile).second)
        {
            before.push_back(SurfaceFlip_t{&rTile, rTile.IsWater()});
        }
        rTile.SetElevation(rWrite.elevation);
    }

    if (pTileEffects)
    {
        std::vector<SurfaceFlip_t> flips;
        for (const SurfaceFlip_t& rBefore : before)
        {
            if (rBefore.pTile->IsWater() != rBefore.bNowWater)
            {
                flips.push_back(SurfaceFlip_t{rBefore.pTile, rBefore.pTile->IsWater()});
            }
        }
        ReconcileSurfaceFlips(*pTileEffects, pWorld, flips);
    }

    RecomputeRivers(rWorldMap);
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

bool ApplyElevationDelta(Tile& rOrigin, WorldMap& rWorldMap, int deltaMeters,
                         const ElevationRulesConfig_t& rRules, int floorMeters, int ceilingMeters,
                         TileEffectsContext* pTileEffects, IUnitOrderWorld* pWorld)
{
    const ElevationSeed_t seed{&rOrigin, deltaMeters};
    const std::vector<ElevationWrite_t> writes = PlanElevationChange(
        std::span<const ElevationSeed_t>(&seed, 1), floorMeters, ceilingMeters, rRules);
    return ApplyElevationWrites_(writes, rWorldMap, pTileEffects, pWorld);
}

bool ApplyEarthquake(Tile& rOrigin, WorldMap& rWorldMap, int levelCount, std::mt19937& rRng,
                     const ElevationRulesConfig_t& rRules, TileEffectsContext* pTileEffects,
                     IUnitOrderWorld* pWorld)
{
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
    return ApplyElevationDelta(rOrigin, rWorldMap, delta, rRules, rRules.minElevationMeters,
                               rRules.maxElevationMeters, pTileEffects, pWorld);
}

bool LowerTilesOneLevel(std::span<Tile*> tiles, WorldMap& rWorldMap, std::mt19937& rRng,
                        const ElevationRulesConfig_t& rRules, TileEffectsContext* pTileEffects,
                        IUnitOrderWorld* pWorld)
{
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
    return ApplyElevationWrites_(writes, rWorldMap, pTileEffects, pWorld);
}

} // namespace ac

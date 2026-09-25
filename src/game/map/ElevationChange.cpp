#include "game/map/ElevationChange.h"

#include "game/map/MapUtils.h"
#include "game/map/RiverGeneration.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <deque>
#include <stdexcept>
#include <unordered_set>

namespace ac
{

namespace
{

int Clamp_(int elevation, int floorMeters, int ceilingMeters)
{
    return std::max(floorMeters, std::min(elevation, ceilingMeters));
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
                         const ElevationRulesConfig_t& rRules, int floorMeters, int ceilingMeters)
{
    if (rRules.maxAdjacentDifferenceMeters <= 0)
    {
        throw std::logic_error("ApplyElevationDelta: max adjacent difference must be > 0");
    }

    // The caller's band intersected with Planet's own, resolved once: the origin honors both,
    // pulled neighbors only Planet's.
    const int originFloor = std::max(floorMeters, rRules.minElevationMeters);
    const int originCeiling = std::min(ceilingMeters, rRules.maxElevationMeters);
    if (originFloor > originCeiling)
    {
        throw std::logic_error("ApplyElevationDelta: the requested band excludes Planet's range");
    }

    const int before = rOrigin.GetElevation();
    const int next = Clamp_(before + deltaMeters, originFloor, originCeiling);
    if (next == before)
    {
        return false;
    }

    rOrigin.SetElevation(next);

    const int maxDiff = rRules.maxAdjacentDifferenceMeters;
    std::deque<Tile*> pending;
    std::unordered_set<Tile*> queued;
    pending.push_back(&rOrigin);
    queued.insert(&rOrigin);

    while (!pending.empty())
    {
        Tile* pTile = pending.front();
        pending.pop_front();
        queued.erase(pTile);

        ForEachTileInChebyshevRadius(*pTile, rWorldMap, 1, false,
            [&](Tile* pNeighbor, int /*distance*/)
            {
                if (!pNeighbor)
                {
                    return;
                }
                const int tileElev = pTile->GetElevation();
                const int neighborElev = pNeighbor->GetElevation();
                int desired = neighborElev;
                if (neighborElev > tileElev + maxDiff)
                {
                    desired = tileElev + maxDiff;
                }
                else if (neighborElev < tileElev - maxDiff)
                {
                    desired = tileElev - maxDiff;
                }
                else
                {
                    return;
                }

                desired = Clamp_(desired, rRules.minElevationMeters, rRules.maxElevationMeters);
                if (desired == neighborElev)
                {
                    return;
                }

                pNeighbor->SetElevation(desired);
                if (queued.insert(pNeighbor).second)
                {
                    pending.push_back(pNeighbor);
                }
            });
    }

    RecomputeRivers(rWorldMap);
    return true;
}

bool ApplyEarthquake(Tile& rOrigin, WorldMap& rWorldMap, int levelCount, std::mt19937& rRng,
                     const ElevationRulesConfig_t& rRules)
{
    if (levelCount <= 0)
    {
        return false;
    }

    // Every level is rolled even once the sum is past anything Planet can absorb, so the RNG
    // stream advances by levelCount whatever the rolls were.
    const int span = rRules.maxElevationMeters - rRules.minElevationMeters;
    int delta = 0;
    for (int level = 0; level < levelCount; ++level)
    {
        const int roll = RollLevelMeters(rRng, rRules);
        delta = roll >= span - delta ? span : delta + roll;
    }

    return ApplyElevationDelta(rOrigin, rWorldMap, delta, rRules, rRules.minElevationMeters,
                               rRules.maxElevationMeters);
}

} // namespace ac

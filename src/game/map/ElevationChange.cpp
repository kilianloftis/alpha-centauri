#include "game/map/ElevationChange.h"

#include "game/effects/TileEffectsContext.h"
#include "game/map/MapUtils.h"
#include "game/map/SurfaceOccupancy.h"
#include "game/map/RiverGeneration.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <deque>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace ac
{

namespace
{

int Clamp_(int elevation, int floorMeters, int ceilingMeters)
{
    return std::max(floorMeters, std::min(elevation, ceilingMeters));
}

// The caller's band intersected with Planet's own. The origin honors both; pulled
// neighbors only Planet's.
struct OriginBand_t
{
    int floor = 0;
    int ceiling = 0;
};

OriginBand_t RequireLegalEdit_(int floorMeters, int ceilingMeters,
                               const ElevationRulesConfig_t& rRules)
{
    if (rRules.maxAdjacentDifferenceMeters <= 0)
    {
        throw std::logic_error("ApplyElevationDelta: max adjacent difference must be > 0");
    }

    OriginBand_t band;
    band.floor = std::max(floorMeters, rRules.minElevationMeters);
    band.ceiling = std::min(ceilingMeters, rRules.maxElevationMeters);
    if (band.floor > band.ceiling)
    {
        throw std::logic_error("ApplyElevationDelta: the requested band excludes Planet's range");
    }
    return band;
}

// Elevation a neighbor must take to sit within maxDiff of tileElev, or nullopt when it
// already does (including after Planet's own clamp).
std::optional<int> PulledElevation_(int tileElev, int neighborElev, int maxDiff, int floorMeters,
                                    int ceilingMeters)
{
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
        return std::nullopt;
    }

    desired = Clamp_(desired, floorMeters, ceilingMeters);
    if (desired == neighborElev)
    {
        return std::nullopt;
    }
    return desired;
}

// First observation wins, so a tile pulled across the line and back records the net flip.
class PriorSurface
{
public:
    explicit PriorSurface(bool bRecord)
        : m_bRecord(bRecord)
    {
    }

    void Note(Tile& rTile)
    {
        if (m_bRecord)
        {
            m_wasWater.emplace(&rTile, rTile.IsWater());
        }
    }

    void AppendFlips(std::vector<SurfaceFlip_t>& rFlips) const
    {
        for (const auto& [pTile, bWasWater] : m_wasWater)
        {
            if (pTile->IsWater() != bWasWater)
            {
                rFlips.push_back(SurfaceFlip_t{pTile, pTile->IsWater()});
            }
        }
    }

    std::vector<Tile*> NotedTiles() const
    {
        std::vector<Tile*> tiles;
        tiles.reserve(m_wasWater.size());
        for (const auto& [pTile, bWasWater] : m_wasWater)
        {
            (void)bWasWater;
            tiles.push_back(pTile);
        }
        return tiles;
    }

private:
    bool m_bRecord = false;
    std::unordered_map<Tile*, bool> m_wasWater;
};

void RelaxAdjacentSlopes_(Tile& rOrigin, WorldMap& rWorldMap, int maxDiff, int floorMeters,
                          int ceilingMeters, PriorSurface& rPrior)
{
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
                const std::optional<int> desired = PulledElevation_(
                    pTile->GetElevation(), pNeighbor->GetElevation(), maxDiff, floorMeters,
                    ceilingMeters);
                if (!desired)
                {
                    return;
                }

                rPrior.Note(*pNeighbor);
                pNeighbor->SetElevation(*desired);
                if (queued.insert(pNeighbor).second)
                {
                    pending.push_back(pNeighbor);
                }
            });
    }
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
    const OriginBand_t band = RequireLegalEdit_(floorMeters, ceilingMeters, rRules);
    const int next = Clamp_(rOrigin.GetElevation() + deltaMeters, band.floor, band.ceiling);
    if (next == rOrigin.GetElevation())
    {
        return false;
    }

    TileChangeDeferral defer;
    PriorSurface prior(pTileEffects != nullptr);
    prior.Note(rOrigin);
    rOrigin.SetElevation(next);
    RelaxAdjacentSlopes_(rOrigin, rWorldMap, rRules.maxAdjacentDifferenceMeters,
                         rRules.minElevationMeters, rRules.maxElevationMeters, prior);
    if (pTileEffects)
    {
        std::vector<SurfaceFlip_t> flips;
        prior.AppendFlips(flips);
        ReconcileSurfaceFlips(*pTileEffects, pWorld, flips);
    }

    RecomputeRivers(rWorldMap);
    return true;
}

bool ApplyEarthquake(Tile& rOrigin, WorldMap& rWorldMap, int levelCount, std::mt19937& rRng,
                     const ElevationRulesConfig_t& rRules, TileEffectsContext* pTileEffects,
                     IUnitOrderWorld* pWorld)
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
                               rRules.maxElevationMeters, pTileEffects, pWorld);
}

} // namespace ac

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
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

void RelaxAdjacentSlopes_(std::span<Tile*> seeds, WorldMap& rWorldMap, int maxDiff,
                          int floorMeters, int ceilingMeters, PriorSurface& rPrior)
{
    std::deque<Tile*> pending;
    std::unordered_set<Tile*> queued;
    for (Tile* pSeed : seeds)
    {
        if (pSeed && queued.insert(pSeed).second)
        {
            pending.push_back(pSeed);
        }
    }

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
    Tile* pOrigin = &rOrigin;
    RelaxAdjacentSlopes_(std::span<Tile*>(&pOrigin, 1), rWorldMap,
                         rRules.maxAdjacentDifferenceMeters, rRules.minElevationMeters,
                         rRules.maxElevationMeters, prior);
    if (pTileEffects)
    {
        std::vector<SurfaceFlip_t> flips;
        prior.AppendFlips(flips);
        ReconcileSurfaceFlips(*pTileEffects, pWorld, flips);
    }

    RecomputeRivers(rWorldMap);
    return true;
}

bool FormerElevationChangeAllowed(const Tile& rTile, UnitDomain_t formerDomain, bool bRaise,
                                  const ElevationRulesConfig_t& rRules)
{
    const bool bSeaFormer = formerDomain == UnitDomain_t::Sea;
    const int elevation = rTile.GetElevation();
    if (bRaise)
    {
        return bSeaFormer ? elevation <= -rRules.referenceLevelMeters
                          : elevation < rRules.maxElevationMeters;
    }
    // TODO: SMAC's floor for sea-former lowering is unknown; Planet's own floor is the only
    // limit we can state.
    return bSeaFormer ? elevation - rRules.referenceLevelMeters >= rRules.minElevationMeters
                      : elevation >= rRules.referenceLevelMeters;
}

int FormerLowerFloorMeters(UnitDomain_t formerDomain, const ElevationRulesConfig_t& rRules)
{
    return formerDomain == UnitDomain_t::Land ? rRules.oceanLevelMeters : rRules.minElevationMeters;
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

bool LowerTilesOneLevel(std::span<Tile*> tiles, WorldMap& rWorldMap, std::mt19937& rRng,
                        const ElevationRulesConfig_t& rRules, TileEffectsContext* pTileEffects,
                        IUnitOrderWorld* pWorld)
{
    if (tiles.empty())
    {
        return false;
    }

    const OriginBand_t band =
        RequireLegalEdit_(rRules.minElevationMeters, rRules.maxElevationMeters, rRules);

    TileChangeDeferral defer;
    PriorSurface prior(pTileEffects != nullptr);
    std::vector<Tile*> changed;
    changed.reserve(tiles.size());

    for (Tile* pTile : tiles)
    {
        if (!pTile)
        {
            continue;
        }
        // Roll even when the tile is already on the floor, so the stream advances once per tile.
        const int roll = RollLevelMeters(rRng, rRules);
        const int next = Clamp_(pTile->GetElevation() - roll, band.floor, band.ceiling);
        if (next == pTile->GetElevation())
        {
            continue;
        }
        prior.Note(*pTile);
        pTile->SetElevation(next);
        changed.push_back(pTile);
    }

    if (changed.empty())
    {
        return false;
    }

    RelaxAdjacentSlopes_(changed, rWorldMap, rRules.maxAdjacentDifferenceMeters,
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

} // namespace ac

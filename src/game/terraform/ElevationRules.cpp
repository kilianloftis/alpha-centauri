#include "game/terraform/ElevationRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/WorldMap.h"
#include "game/movement/MovementRules.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ac
{

int QuoteRaiseLowerEnergyCost(const Tile& rTile, FactionId_t factionId)
{
    const MapRulesConfig_t& rRules = rTile.MapRules();
    if (rRules.referenceLevelMeters <= 0)
    {
        throw std::logic_error("QuoteRaiseLowerEnergyCost: reference_level_meters must be > 0");
    }
    const WorldMap& rWorldMap = CurrentGameState().GetWorldMap();
    const int elevBand = std::abs(rTile.GetElevation()) / rRules.referenceLevelMeters + 1;
    int nearest = std::numeric_limits<int>::max();
    const int mapWidth = rWorldMap.GetWidth();

    for (const auto& pTile : rWorldMap.GetTiles())
    {
        if (!pTile || !pTile->HasImprovement(ImprovementIds::k_Base))
        {
            continue;
        }
        if (rWorldMap.GetTerritory().GetOwner(*pTile) == factionId)
        {
            nearest = std::min(nearest, ChebyshevDistance(rTile, *pTile, mapWidth));
        }
    }
    if (nearest == std::numeric_limits<int>::max())
    {
        nearest = std::max(rWorldMap.GetWidth(), rWorldMap.GetHeight());
    }
    return elevBand * 8 + nearest * 2;
}

int FormerElevationAfterMaxLevel(const Tile& rTile, bool bRaise, const MapRulesConfig_t& rRules)
{
    const int delta = bRaise ? rRules.levelMaxMeters : -rRules.levelMaxMeters;
    return std::clamp(rTile.GetElevation() + delta, rRules.minElevationMeters,
                      rRules.maxElevationMeters);
}

bool FormerElevationChangeAllowed(const Unit& rUnit, bool bRaise, const MapRulesConfig_t& rRules)
{
    const Tile& rTile = rUnit.GetTile();
    const int elevation = rTile.GetElevation();
    if (bRaise)
    {
        // TOOHIGH: SMAC refuses raise from altitude 6 — the top reference band under Planet's
        // ceiling (docs/thinker/terranx-map.md "Starting a project").
        const int topBandStart = rRules.maxElevationMeters - rRules.referenceLevelMeters;
        if (elevation >= topBandStart)
        {
            return false;
        }
    }
    const int after = FormerElevationAfterMaxLevel(rTile, bRaise, rRules);
    if (after == elevation)
    {
        return false;
    }
    const InteractionSurface_t surface =
        after < rRules.oceanLevelMeters ? InteractionSurface_t::Water : InteractionSurface_t::Land;
    return CanEnterSurface(rUnit, surface);
}

namespace
{

int ClampElevation_(int elevation, int floorMeters, int ceilingMeters)
{
    return std::max(floorMeters, std::min(elevation, ceilingMeters));
}

// The caller's band intersected with Planet's own. Seeds honor both; pulled neighbors only
// Planet's.
struct ElevationBand_t
{
    int floor = 0;
    int ceiling = 0;
};

ElevationBand_t RequireLegalEdit_(int floorMeters, int ceilingMeters,
                                  const MapRulesConfig_t& rRules)
{
    if (rRules.maxAdjacentDifferenceMeters <= 0)
    {
        throw std::logic_error("PlanElevationChange: max adjacent difference must be > 0");
    }
    ElevationBand_t band;
    band.floor = std::max(floorMeters, rRules.minElevationMeters);
    band.ceiling = std::min(ceilingMeters, rRules.maxElevationMeters);
    if (band.floor > band.ceiling)
    {
        throw std::logic_error("PlanElevationChange: the requested band excludes Planet's range");
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

    desired = ClampElevation_(desired, floorMeters, ceilingMeters);
    if (desired == neighborElev)
    {
        return std::nullopt;
    }
    return desired;
}

// Elevations as the planned writes leave them, with the writes kept in order.
class WorkingElevations_
{
public:
    int Get(const Tile& rTile) const
    {
        const auto it = m_elevations.find(&rTile);
        return it == m_elevations.end() ? rTile.GetElevation() : it->second;
    }

    void Set(const Tile& rTile, int elevation)
    {
        m_elevations[&rTile] = elevation;
        m_writes.push_back(ElevationWrite_t{&rTile, elevation});
    }

    std::vector<ElevationWrite_t> TakeWrites() { return std::move(m_writes); }

private:
    std::unordered_map<const Tile*, int> m_elevations;
    std::vector<ElevationWrite_t> m_writes;
};

void RelaxAdjacentSlopes_(const std::vector<const Tile*>& rSeeds, const WorldMap& rWorldMap,
                          const MapRulesConfig_t& rRules, WorkingElevations_& rWorking)
{
    std::deque<const Tile*> pending;
    std::unordered_set<const Tile*> queued;
    for (const Tile* pSeed : rSeeds)
    {
        if (queued.insert(pSeed).second)
        {
            pending.push_back(pSeed);
        }
    }

    while (!pending.empty())
    {
        const Tile* pTile = pending.front();
        pending.pop_front();
        queued.erase(pTile);

        ForEachNeighborInChebyshevRadius(*pTile, rWorldMap, 1,
            [&](const Tile* pNeighbor, int /*distance*/)
            {
                const int tileElev = rWorking.Get(*pTile);
                const int neighborElev = rWorking.Get(*pNeighbor);
                const std::optional<int> desired = PulledElevation_(
                    tileElev, neighborElev, rRules.maxAdjacentDifferenceMeters,
                    rRules.minElevationMeters, rRules.maxElevationMeters);
                if (!desired)
                {
                    return;
                }
                rWorking.Set(*pNeighbor, *desired);
                if (queued.insert(pNeighbor).second)
                {
                    pending.push_back(pNeighbor);
                }
            });
    }
}

// The seeded tiles whose clamped elevation actually changed.
std::vector<const Tile*> ApplySeeds_(std::span<const ElevationSeed_t> seeds,
                                     const ElevationBand_t& rBand, WorkingElevations_& rWorking)
{
    std::vector<const Tile*> moved;
    for (const ElevationSeed_t& rSeed : seeds)
    {
        const int current = rWorking.Get(*rSeed.pTile);
        const int next = ClampElevation_(current + rSeed.deltaMeters, rBand.floor, rBand.ceiling);
        if (next == current)
        {
            continue;
        }
        rWorking.Set(*rSeed.pTile, next);
        moved.push_back(rSeed.pTile);
    }
    return moved;
}

} // namespace

std::vector<ElevationWrite_t> PlanElevationChange(std::span<const ElevationSeed_t> seeds,
                                                  int floorMeters, int ceilingMeters,
                                                  const MapRulesConfig_t& rRules)
{
    const ElevationBand_t band = RequireLegalEdit_(floorMeters, ceilingMeters, rRules);
    const WorldMap& rWorldMap = CurrentGameState().GetWorldMap();
    WorkingElevations_ working;
    const std::vector<const Tile*> moved = ApplySeeds_(seeds, band, working);
    if (!moved.empty())
    {
        RelaxAdjacentSlopes_(moved, rWorldMap, rRules, working);
    }
    return working.TakeWrites();
}

int EarthquakeDeltaMeters(std::span<const int> levelRollsMeters,
                          const MapRulesConfig_t& rRules)
{
    const int span = rRules.maxElevationMeters - rRules.minElevationMeters;
    int delta = 0;
    for (const int roll : levelRollsMeters)
    {
        delta = roll >= span - delta ? span : delta + roll;
    }
    return delta;
}

} // namespace ac

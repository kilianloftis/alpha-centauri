#include "game/units/Pathfinder.h"

#include "game/map/MapUtils.h"
#include "game/units/MoveCostCalculator.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace ac
{

namespace
{

bool IsDesiredContactCandidate_(StepOutcome_t outcome)
{
    return outcome == StepOutcome_t::Legal
        || outcome == StepOutcome_t::BlockedByOccupant
        || outcome == StepOutcome_t::BlockedByZoc;
}

} // namespace

Pathfinder::Pathfinder(const MoveCostCalculator& rMoveCosts,
                       const StepEvaluator& rSteps,
                       WorldMap& rWorldMap)
    : m_rMoveCosts(rMoveCosts)
    , m_rSteps(rSteps)
    , m_rWorldMap(rWorldMap)
{
}

Path_t Pathfinder::FindPath(const Unit& rMover, const Tile& rDestination) const
{
    Path_t result;
    const Tile& rStart = rMover.GetTile();
    if (&rStart == &rDestination)
    {
        result.bReachable = true;
        return result;
    }

    // Known domain mismatch (e.g. land unit → explored water): unreachable without a
    // full-component Dijkstra. Shrouded tiles still pass CanPlanEnterTerrain.
    if (!m_rSteps.CanPlanEnterTerrain(rMover, rDestination))
    {
        return result;
    }

    const int tileCount = m_rWorldMap.GetWidth() * m_rWorldMap.GetHeight();
    constexpr int k_inf = std::numeric_limits<int>::max();

    // Cost is (forbidden-territory tiles entered, move fragments), compared in that order: a
    // route through territory the mover may not enter is taken only when no route avoids it,
    // and the step onto such a tile is what asks the player (BlockedByTerritory).
    using Cost_t = std::pair<int, int>;
    const Cost_t k_unreached{k_inf, k_inf};
    std::vector<Cost_t> dist(static_cast<size_t>(tileCount), k_unreached);
    std::vector<int> parent(static_cast<size_t>(tileCount), -1);
    const auto& tiles = m_rWorldMap.GetTiles();

    const int startIdx = m_rWorldMap.GetTileIndex(rStart);
    const int destIdx = m_rWorldMap.GetTileIndex(rDestination);
    dist[static_cast<size_t>(startIdx)] = Cost_t{0, 0};

    const auto costs = m_rMoveCosts.ForUnit(rMover, m_rWorldMap);

    // Min-heap of (cost, tileIndex).
    using Node_t = std::pair<Cost_t, int>;
    std::priority_queue<Node_t, std::vector<Node_t>, std::greater<Node_t>> open;
    open.push({Cost_t{0, 0}, startIdx});

    while (!open.empty())
    {
        const auto [cost, idx] = open.top();
        open.pop();
        if (cost > dist[static_cast<size_t>(idx)])
        {
            continue;
        }
        if (idx == destIdx)
        {
            break;
        }

        const Tile* pFrom = tiles[static_cast<size_t>(idx)].get();
        if (!pFrom)
        {
            continue;
        }

        ForEachTileInChebyshevRadius(*pFrom, m_rWorldMap, /*radius=*/1, /*includeOrigin=*/false,
            [&](const Tile* pNeighbor, int /*distance*/)
            {
                if (!m_rSteps.CanPlanStep(rMover, *pFrom, *pNeighbor))
                {
                    return;
                }
                const int edgeCost = costs.PlannedCostFragments(*pNeighbor);
                // MagTube (0) is allowed; still advance so the search progresses.
                const Cost_t newCost{
                    cost.first + (m_rSteps.IsForbiddenTerritory(rMover, *pNeighbor) ? 1 : 0),
                    cost.second + edgeCost};
                if (newCost.second < 0)
                {
                    // Overflow guard for pathological sums; treat as unreachable via this edge.
                    return;
                }
                const int neighborIdx = m_rWorldMap.GetTileIndex(*pNeighbor);
                if (newCost < dist[static_cast<size_t>(neighborIdx)])
                {
                    dist[static_cast<size_t>(neighborIdx)] = newCost;
                    parent[static_cast<size_t>(neighborIdx)] = idx;
                    open.push({newCost, neighborIdx});
                }
            });
    }

    if (dist[static_cast<size_t>(destIdx)] == k_unreached)
    {
        return result;
    }

    // Reconstruct path destination -> start, then reverse into result.tiles.
    std::vector<const Tile*> reversed;
    for (int idx = destIdx; idx != startIdx; idx = parent[static_cast<size_t>(idx)])
    {
        if (idx < 0)
        {
            return Path_t{};
        }
        const Tile* pTile = tiles[static_cast<size_t>(idx)].get();
        if (!pTile)
        {
            return Path_t{};
        }
        reversed.push_back(pTile);
    }

    result.tiles.assign(reversed.rbegin(), reversed.rend());
    result.totalCostFragments = dist[static_cast<size_t>(destIdx)].second;
    result.forbiddenTerritoryTiles = dist[static_cast<size_t>(destIdx)].first;
    result.bReachable = true;
    return result;
}

const Tile* Pathfinder::NextStep(const Unit& rMover, const Tile& rDestination) const
{
    const Path_t path = FindPath(rMover, rDestination);
    if (!path.bReachable || path.tiles.empty())
    {
        return nullptr;
    }
    return path.tiles.front();
}

const Tile* Pathfinder::DesiredContactStep(const Unit& rMover, const Tile& rDestination) const
{
    const Tile& rFrom = rMover.GetTile();
    if (&rFrom == &rDestination)
    {
        return nullptr;
    }

    const int currentDist = ChebyshevDistance(rFrom, rDestination, m_rWorldMap.GetWidth());
    const Tile* pBest = nullptr;
    int bestDist = currentDist;

    ForEachTileInChebyshevRadius(rFrom, m_rWorldMap, /*radius=*/1, /*includeOrigin=*/false,
        [&](const Tile* pTile, int /*distance*/)
        {
            const StepEvaluation_t eval = m_rSteps.EvaluateStep(rMover, rFrom, *pTile);
            if (!IsDesiredContactCandidate_(eval.outcome))
            {
                return;
            }
            const int dist = ChebyshevDistance(*pTile, rDestination, m_rWorldMap.GetWidth());
            if (dist < bestDist)
            {
                bestDist = dist;
                pBest = pTile;
            }
        });

    return pBest;
}

} // namespace ac

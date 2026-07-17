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

    const int tileCount = m_rWorldMap.GetWidth() * m_rWorldMap.GetHeight();
    constexpr int k_inf = std::numeric_limits<int>::max();

    std::vector<int> dist(static_cast<size_t>(tileCount), k_inf);
    std::vector<int> parent(static_cast<size_t>(tileCount), -1);
    const auto& tiles = m_rWorldMap.GetTiles();

    const int startIdx = m_rWorldMap.GetTileIndex(rStart);
    const int destIdx = m_rWorldMap.GetTileIndex(rDestination);
    dist[static_cast<size_t>(startIdx)] = 0;

    const auto costs = m_rMoveCosts.ForUnit(rMover, m_rWorldMap);

    // Min-heap of (cost, tileIndex).
    using Node_t = std::pair<int, int>;
    std::priority_queue<Node_t, std::vector<Node_t>, std::greater<Node_t>> open;
    open.push({0, startIdx});

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
                const int newCost = cost + edgeCost;
                if (newCost < 0)
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

    if (dist[static_cast<size_t>(destIdx)] == k_inf)
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
    result.totalCostFragments = dist[static_cast<size_t>(destIdx)];
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

    const int currentDist = ChebyshevDistance(rFrom, rDestination);
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
            const int dist = ChebyshevDistance(*pTile, rDestination);
            if (dist < bestDist)
            {
                bestDist = dist;
                pBest = pTile;
            }
        });

    return pBest;
}

} // namespace ac

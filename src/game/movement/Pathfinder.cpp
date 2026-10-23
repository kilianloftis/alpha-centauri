#include "game/movement/Pathfinder.h"

#include "game/map/MapUtils.h"
#include "game/movement/MoveCostRules.h"
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

constexpr int k_Inf = std::numeric_limits<int>::max();

bool IsDesiredContactCandidate_(StepOutcome_t outcome)
{
    return outcome == StepOutcome_t::Legal
        || outcome == StepOutcome_t::BlockedByOccupant
        || outcome == StepOutcome_t::BlockedByZoc;
}

} // namespace

Pathfinder::Pathfinder(const MoveCostRules& rMoveCosts,
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

    const int startIdx = m_rWorldMap.GetTileIndex(rStart);
    const int destIdx = m_rWorldMap.GetTileIndex(rDestination);
    const SearchTree_t tree = Search_(rMover, startIdx, destIdx);
    const Cost_t& rDestCost = tree.dist[static_cast<size_t>(destIdx)];
    if (rDestCost == Cost_t{k_Inf, k_Inf})
    {
        return result;
    }
    std::optional<std::vector<const Tile*>> tiles = TraceBack_(tree, startIdx, destIdx);
    if (!tiles)
    {
        return Path_t{};
    }
    result.tiles = std::move(*tiles);
    result.totalCostFragments = rDestCost.second;
    result.forbiddenTerritoryTiles = rDestCost.first;
    result.bReachable = true;
    return result;
}

Pathfinder::SearchTree_t Pathfinder::Search_(const Unit& rMover, int startIdx, int destIdx) const
{
    const size_t tileCount = m_rWorldMap.GetTiles().size();
    SearchTree_t tree{std::vector<Cost_t>(tileCount, Cost_t{k_Inf, k_Inf}),
                      std::vector<int>(tileCount, -1)};
    tree.dist[static_cast<size_t>(startIdx)] = Cost_t{0, 0};
    const auto& tiles = m_rWorldMap.GetTiles();
    const auto costs = m_rMoveCosts.ForUnit(rMover, m_rWorldMap);

    // Min-heap of (cost, tileIndex).
    using Node_t = std::pair<Cost_t, int>;
    std::priority_queue<Node_t, std::vector<Node_t>, std::greater<Node_t>> open;
    open.push({Cost_t{0, 0}, startIdx});
    while (!open.empty())
    {
        const auto [cost, idx] = open.top();
        open.pop();
        if (cost > tree.dist[static_cast<size_t>(idx)])
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
        ForEachNeighborInChebyshevRadius(*pFrom, m_rWorldMap, /*radius=*/1,
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
                const size_t neighborIdx =
                    static_cast<size_t>(m_rWorldMap.GetTileIndex(*pNeighbor));
                if (newCost < tree.dist[neighborIdx])
                {
                    tree.dist[neighborIdx] = newCost;
                    tree.parent[neighborIdx] = idx;
                    open.push({newCost, static_cast<int>(neighborIdx)});
                }
            });
    }
    return tree;
}

std::optional<std::vector<const Tile*>> Pathfinder::TraceBack_(const SearchTree_t& rTree,
                                                               int startIdx, int destIdx) const
{
    const auto& tiles = m_rWorldMap.GetTiles();
    std::vector<const Tile*> reversed;
    for (int idx = destIdx; idx != startIdx; idx = rTree.parent[static_cast<size_t>(idx)])
    {
        if (idx < 0)
        {
            return std::nullopt;
        }
        const Tile* pTile = tiles[static_cast<size_t>(idx)].get();
        if (!pTile)
        {
            return std::nullopt;
        }
        reversed.push_back(pTile);
    }
    return std::vector<const Tile*>(reversed.rbegin(), reversed.rend());
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

    ForEachNeighborInChebyshevRadius(rFrom, m_rWorldMap, /*radius=*/1,
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

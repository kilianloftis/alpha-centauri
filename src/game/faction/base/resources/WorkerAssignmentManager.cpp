#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/map/Tile.h"
#include <algorithm>

namespace ac
{

namespace
{

constexpr bool IsUnassignedTile(const TileCoord& rCoord)
{
    return rCoord.first == -1 && rCoord.second == -1;
}

} // namespace

WorkerAssignmentManager::WorkerAssignmentManager(std::vector<const Tile*> workableTiles)
    : m_workableTiles(std::move(workableTiles))
    , m_scorer([](const Tile& rTile) -> float
      {
          return static_cast<float>(rTile.GetNutrientProduction()
                                   + rTile.GetEnergyProduction()
                                   + rTile.GetMineralProduction());
      })
{
}

bool WorkerAssignmentManager::AssignWorker(Pop& rPop, int x, int y, PopContainer& rPops)
{
    if (!rPop.IsWorker())
    {
        return false;
    }

    if (!IsTileWorkable_(x, y))
    {
        return false;
    }

    if (IsTileAssigned_(x, y, rPops))
    {
        return false;
    }

    rPop.SetTileCoord(x, y);
    return true;
}

void WorkerAssignmentManager::UnassignWorker(Pop& rPop)
{
    rPop.SetTileCoord(-1, -1);
}

void WorkerAssignmentManager::UnassignAll(PopContainer& rPops)
{
    for (auto& pPop : rPops.GetPops())
    {
        pPop->SetTileCoord(-1, -1);
    }
}

bool WorkerAssignmentManager::IsTileAssigned(int x, int y, const PopContainer& rPops) const
{
    return IsTileAssigned_(x, y, rPops);
}

TileResources_t WorkerAssignmentManager::ComputeWorkedResources(
    const PopContainer& rPops, const TileLookup& tileAt) const
{
    TileResources_t total{0, 0, 0};
    for (const auto& pPop : rPops.GetPops())
    {
        if (!pPop->IsWorker())
        {
            continue;
        }

        const TileCoord coord = pPop->GetTileCoord();
        if (IsUnassignedTile(coord))
        {
            continue;
        }

        const Tile* pTile = tileAt(coord.first, coord.second);
        if (!pTile)
        {
            continue;
        }

        const TileResources_t raw{
            pTile->GetNutrientProduction(),
            pTile->GetEnergyProduction(),
            pTile->GetMineralProduction()
        };
        const TileResources_t modified = pPop->ApplyTileMultipliers(raw);

        total.nutrients += modified.nutrients;
        total.energy    += modified.energy;
        total.minerals  += modified.minerals;
    }
    return total;
}

void WorkerAssignmentManager::SetWorkableTiles(std::vector<const Tile*> workableTiles, PopContainer& rPops)
{
    m_workableTiles = std::move(workableTiles);

    for (auto& pPop : rPops.GetPops())
    {
        const TileCoord coord = pPop->GetTileCoord();
        if (!IsUnassignedTile(coord) && !IsTileWorkable_(coord.first, coord.second))
        {
            pPop->SetTileCoord(-1, -1);
        }
    }
}

void WorkerAssignmentManager::SetTileScorer(TileScorer scorer)
{
    m_scorer = std::move(scorer);
}

const std::vector<const Tile*>& WorkerAssignmentManager::GetWorkableTiles() const
{
    return m_workableTiles;
}

void WorkerAssignmentManager::AutoAssignWorkers(PopContainer& rPops)
{
    auto unassignedWorkers = GetUnassignedWorkers_(rPops);
    if (unassignedWorkers.empty())
    {
        return;
    }

    auto availableTiles = GetAvailableTiles_(rPops);
    auto prioritizedTiles = PrioritizeAvailableTiles_(availableTiles);
    AutoAssignWorkers_(unassignedWorkers, prioritizedTiles, rPops);
}

std::vector<Pop*> WorkerAssignmentManager::GetUnassignedWorkers_(const PopContainer& rPops) const
{
    std::vector<Pop*> unassignedWorkers;
    for (const auto& pPop : rPops.GetPops())
    {
        if (pPop->IsWorker() && IsUnassignedTile(pPop->GetTileCoord()))
        {
            unassignedWorkers.push_back(pPop.get());
        }
    }
    return unassignedWorkers;
}

std::vector<const Tile*> WorkerAssignmentManager::GetAvailableTiles_(const PopContainer& rPops) const
{
    std::vector<const Tile*> availableTiles;
    for (const Tile* pTile : m_workableTiles)
    {
        if (pTile && !IsTileAssigned_(pTile->GetX(), pTile->GetY(), rPops))
        {
            availableTiles.push_back(pTile);
        }
    }
    return availableTiles;
}

std::vector<const Tile*> WorkerAssignmentManager::PrioritizeAvailableTiles_(
    const std::vector<const Tile*>& availableTiles) const
{
    std::vector<const Tile*> prioritizedTiles = availableTiles;

    std::sort(prioritizedTiles.begin(), prioritizedTiles.end(),
        [this](const Tile* pA, const Tile* pB)
        {
            const float scoreA = pA ? m_scorer(*pA) : 0.0f;
            const float scoreB = pB ? m_scorer(*pB) : 0.0f;
            return scoreA > scoreB;
        });

    return prioritizedTiles;
}

void WorkerAssignmentManager::AutoAssignWorkers_(const std::vector<Pop*>& unassignedWorkers,
                                                 const std::vector<const Tile*>& availableTiles,
                                                 PopContainer& rPops)
{
    size_t workerIndex = 0;
    size_t tileIndex = 0;
    while (workerIndex < unassignedWorkers.size() && tileIndex < availableTiles.size())
    {
        Pop* pPop = unassignedWorkers[workerIndex];
        const Tile* pTile = availableTiles[tileIndex];

        if (pPop && pTile && AssignWorker(*pPop, pTile->GetX(), pTile->GetY(), rPops))
        {
            ++workerIndex;
            ++tileIndex;
        }
        else
        {
            // If assignment failed, try next tile
            ++tileIndex;
        }
    }
}

bool WorkerAssignmentManager::IsTileAssigned_(int x, int y, const PopContainer& rPops) const
{
    for (const auto& pPop : rPops.GetPops())
    {
        if (pPop->IsWorker())
        {
            const TileCoord coord = pPop->GetTileCoord();
            if (coord.first == x && coord.second == y)
            {
                return true;
            }
        }
    }
    return false;
}

bool WorkerAssignmentManager::IsTileWorkable_(int x, int y) const
{
    for (const Tile* pTile : m_workableTiles)
    {
        if (pTile && pTile->GetX() == x && pTile->GetY() == y)
        {
            return true;
        }
    }
    return false;
}

} // namespace ac

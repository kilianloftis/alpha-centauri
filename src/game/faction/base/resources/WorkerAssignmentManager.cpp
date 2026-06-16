#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/map/Tile.h"
#include <algorithm>

namespace ac
{

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

bool WorkerAssignmentManager::AssignWorker(int popId, int x, int y, const PopContainer& rPops)
{
    const Pop* pPop = FindPop_(popId, rPops);
    if (!pPop || !pPop->IsWorker())
    {
        return false;
    }

    if (IsTileAssigned(x, y))
    {
        return false;
    }

    m_assignments[popId] = {x, y};
    return true;
}

void WorkerAssignmentManager::UnassignWorker(int popId)
{
    m_assignments.erase(popId);
}

void WorkerAssignmentManager::UnassignAll()
{
    m_assignments.clear();
}

bool WorkerAssignmentManager::IsTileAssigned(int x, int y) const
{
    for (const auto& rEntry : m_assignments)
    {
        if (rEntry.second.first == x && rEntry.second.second == y)
        {
            return true;
        }
    }
    return false;
}

WorkerAssignmentManager::TileCoord WorkerAssignmentManager::GetAssignedTile(int popId) const
{
    auto it = m_assignments.find(popId);
    if (it != m_assignments.end())
    {
        return it->second;
    }
    return {-1, -1};
}

const std::unordered_map<int, WorkerAssignmentManager::TileCoord>&
WorkerAssignmentManager::GetAssignments() const
{
    return m_assignments;
}

TileResources_t WorkerAssignmentManager::ComputeWorkedResources(
    const PopContainer& rPops, const TileLookup& tileAt) const
{
    TileResources_t total{0, 0, 0};
    for (const auto& rEntry : m_assignments)
    {
        const int popId = rEntry.first;
        const int x     = rEntry.second.first;
        const int y     = rEntry.second.second;

        const Pop* pPop = FindPop_(popId, rPops);
        if (!pPop || !pPop->IsWorker())
        {
            continue;
        }

        const Tile* pTile = tileAt(x, y);
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

void WorkerAssignmentManager::SetWorkableTiles(std::vector<const Tile*> workableTiles)
{
    m_workableTiles = std::move(workableTiles);
}

void WorkerAssignmentManager::SetTileScorer(TileScorer scorer)
{
    m_scorer = std::move(scorer);
}

void WorkerAssignmentManager::AutoAssignWorkers(const PopContainer& rPops)
{
    auto unassignedWorkerIds = GetUnassignedWorkers_(rPops);
    if (unassignedWorkerIds.empty())
    {
        return;
    }

    auto availableTiles = GetAvailableTiles_();
    auto prioritizedTiles = PrioritizeAvailableTiles_(availableTiles);
    AutoAssignWorkers_(unassignedWorkerIds, prioritizedTiles, rPops);
}

std::vector<int> WorkerAssignmentManager::GetUnassignedWorkers_(const PopContainer& rPops) const
{
    std::vector<int> unassignedWorkerIds;
    for (const auto& pPop : rPops.GetPops())
    {
        if (pPop->IsWorker() && GetAssignedTile(pPop->GetId()).first == -1)
        {
            unassignedWorkerIds.push_back(pPop->GetId());
        }
    }
    return unassignedWorkerIds;
}

std::vector<const Tile*> WorkerAssignmentManager::GetAvailableTiles_() const
{
    std::vector<const Tile*> availableTiles;
    for (const Tile* pTile : m_workableTiles)
    {
        if (pTile && !IsTileAssigned(pTile->GetX(), pTile->GetY()))
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

void WorkerAssignmentManager::AutoAssignWorkers_(const std::vector<int>& unassignedWorkerIds,
                                                 const std::vector<const Tile*>& availableTiles,
                                                 const PopContainer& rPops)
{
    size_t workerIndex = 0;
    size_t tileIndex = 0;
    while (workerIndex < unassignedWorkerIds.size() && tileIndex < availableTiles.size())
    {
        int popId = unassignedWorkerIds[workerIndex];
        const Tile* pTile = availableTiles[tileIndex];

        if (pTile && AssignWorker(popId, pTile->GetX(), pTile->GetY(), rPops))
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

const Pop* WorkerAssignmentManager::FindPop_(int popId, const PopContainer& rPops) const
{
    for (const auto& pPop : rPops.GetPops())
    {
        if (pPop->GetId() == popId)
        {
            return pPop.get();
        }
    }
    return nullptr;
}

} // namespace ac

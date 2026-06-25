#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/map/Tile.h"
#include <algorithm>

namespace ac
{

namespace
{

constexpr bool IsUnassignedTile(const Tile* pTile)
{
    return pTile == nullptr;
}

const Tile* FindWorkableTile_(int x, int y, const std::vector<const Tile*>& workableTiles)
{
    for (const Tile* pTile : workableTiles)
    {
        if (pTile && pTile->GetX() == x && pTile->GetY() == y)
        {
            return pTile;
        }
    }
    return nullptr;
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

bool WorkerAssignmentManager::UserAssignWorker(Pop& rPop, int x, int y, PopContainer& rPops)
{
    if (!AssignWorker(rPop, x, y, rPops))
    {
        return false;
    }
    rPop.SetUserAssigned(true);
    return true;
}

void WorkerAssignmentManager::UserUnassignTile(int x, int y, PopContainer& rPops)
{
    for (auto& pPop : rPops.GetPops())
    {
        if (pPop->IsWorker())
        {
            const Tile* pTile = pPop->GetTile();
            if (pTile && pTile->GetX() == x && pTile->GetY() == y)
            {
                pPop->SetTile(nullptr);
                break;
            }
        }
    }
}

void WorkerAssignmentManager::UserUnassignAll(PopContainer& rPops)
{
    for (auto& pPop : rPops.GetPops())
    {
        if (pPop->IsWorker() && pPop->IsUserAssigned())
        {
            pPop->SetTile(nullptr);
        }
    }
}

bool WorkerAssignmentManager::AssignWorker(Pop& rPop, int x, int y, PopContainer& rPops)
{
    if (!rPop.IsWorker())
    {
        return false;
    }

    const Tile* pTile = FindWorkableTile_(x, y, m_workableTiles);
    if (!pTile)
    {
        return false;
    }

    if (IsTileAssigned_(x, y, rPops))
    {
        return false;
    }

    rPop.SetTile(pTile);
    return true;
}

void WorkerAssignmentManager::UnassignWorker(Pop& rPop)
{
    rPop.SetTile(nullptr);
}

void WorkerAssignmentManager::UnassignAll(PopContainer& rPops)
{
    for (auto& pPop : rPops.GetPops())
    {
        const Tile* pTile = pPop->GetTile();
        if (pTile && !pPop->IsUserAssigned())
        {
            pPop->SetTile(nullptr);
        }
    }
}

bool WorkerAssignmentManager::IsTileAssigned(int x, int y, const PopContainer& rPops) const
{
    return IsTileAssigned_(x, y, rPops);
}

TileResources_t WorkerAssignmentManager::ComputeWorkedResources(
    const PopContainer& rPops) const
{
    TileResources_t total{0, 0, 0};
    for (const auto& pPop : rPops.GetPops())
    {
        if (!pPop->IsWorker())
        {
            continue;
        }

        const Tile* pTile = pPop->GetTile();
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
        const Tile* pTile = pPop->GetTile();
        if (pTile && !IsTileWorkable_(pTile->GetX(), pTile->GetY()))
        {
            pPop->SetTile(nullptr);
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
        if (!pPop->IsWorker())
        {
            continue;
        }
        const Tile* pTile = pPop->GetTile();
        if (IsUnassignedTile(pTile) && !pPop->IsUserAssigned())
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
            const Tile* pTile = pPop->GetTile();
            if (pTile && pTile->GetX() == x && pTile->GetY() == y)
            {
                return true;
            }
        }
    }
    return false;
}

bool WorkerAssignmentManager::IsTileWorkable_(int x, int y) const
{
    return FindWorkableTile_(x, y, m_workableTiles) != nullptr;
}

} // namespace ac

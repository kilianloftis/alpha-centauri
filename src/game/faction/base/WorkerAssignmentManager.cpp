#include "game/faction/base/WorkerAssignmentManager.h"
#include "game/faction/population/PopContainer.h"
#include "game/map/Tile.h"

namespace ac
{

WorkerAssignmentManager::WorkerAssignmentManager()
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

void WorkerAssignmentManager::OnPopulationChanged(const PopContainer& rPops)
{
    std::vector<int> toRemove;
    for (const auto& rEntry : m_assignments)
    {
        const Pop* pPop = FindPop_(rEntry.first, rPops);
        if (!pPop || !pPop->IsWorker())
        {
            toRemove.push_back(rEntry.first);
        }
    }
    for (int popId : toRemove)
    {
        m_assignments.erase(popId);
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

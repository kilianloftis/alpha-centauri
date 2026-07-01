#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/map/Tile.h"
#include "lib/effects/TileEffectsContext.h"
#include <algorithm>

namespace ac
{

namespace
{

constexpr bool IsUnassignedTile(const Tile* pTile)
{
    return pTile == nullptr;
}

} // namespace

WorkerAssignmentManager::WorkerAssignmentManager(std::vector<const Tile*> workableTiles, PopContainer& rPops,
                                                 const TileEffectsContext& rTileEffects)
    : m_workableTiles(std::move(workableTiles))
    , m_scorer([this](const Tile& rTile) -> float
      {
          const TileResources_t yield = m_rTileEffects.ResolveTileYield(rTile);
          return static_cast<float>(yield.nutrients + yield.energy + yield.minerals);
      })
    , m_rPops(rPops)
    , m_rTileEffects(rTileEffects)
{
}

bool WorkerAssignmentManager::UserAssignWorker(Pop& rPop, const Tile* pTile)
{
    if (!AssignWorker(rPop, pTile))
    {
        return false;
    }
    rPop.SetUserAssigned(true);
    m_pLastAssigned = &rPop;
    return true;
}

void WorkerAssignmentManager::UserUnassignTile(const Tile* pTile)
{
    for (auto& pPop : m_rPops.GetPops())
    {
        if (pPop->IsWorker() && pPop->GetTile() == pTile)
        {
            UnassignFromTile_(*pPop);
            break;
        }
    }
}

void WorkerAssignmentManager::ReleaseUserAssignment(Pop& rPop)
{
    rPop.SetTile(nullptr);
}

void WorkerAssignmentManager::ReleaseAllUserAssignments()
{
    for (auto& pPop : m_rPops.GetPops())
    {
        if (pPop->IsWorker() && pPop->IsUserAssigned())
        {
            ReleaseUserAssignment(*pPop);
        }
    }
}

bool WorkerAssignmentManager::AssignWorker(Pop& rPop, const Tile* pTile)
{
    if (!rPop.IsWorker())
    {
        return false;
    }

    if (std::find(m_workableTiles.begin(), m_workableTiles.end(), pTile) == m_workableTiles.end())
    {
        return false;
    }

    if (IsTileAssigned(pTile))
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

void WorkerAssignmentManager::UnassignAll()
{
    for (auto& pPop : m_rPops.GetPops())
    {
        const Tile* pTile = pPop->GetTile();
        if (pTile && !pPop->IsUserAssigned())
        {
            pPop->SetTile(nullptr);
        }
    }
}

void WorkerAssignmentManager::ResetAllAssignments()
{
    for (auto& pPop : m_rPops.GetPops())
    {
        UnassignWorker(*pPop);
    }

    AutoAssignWorkers();
}

bool WorkerAssignmentManager::IsTileAssigned(const Tile* pTile) const
{
    for (const auto& pPop : m_rPops.GetPops())
    {
        if (pPop->IsWorker() && pPop->GetTile() == pTile)
        {
            return true;
        }
    }
    return false;
}

TileResources_t WorkerAssignmentManager::ComputeWorkedResources() const
{
    TileResources_t total{0, 0, 0};
    for (const auto& pPop : m_rPops.GetPops())
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

        const TileResources_t raw = m_rTileEffects.ResolveTileYield(*pTile);
        const TileResources_t modified = pPop->ApplyTileMultipliers(raw);

        total.nutrients += modified.nutrients;
        total.energy    += modified.energy;
        total.minerals  += modified.minerals;
    }
    return total;
}

void WorkerAssignmentManager::SetTileScorer(TileScorer scorer)
{
    m_scorer = std::move(scorer);
}

const std::vector<const Tile*>& WorkerAssignmentManager::GetWorkableTiles() const
{
    return m_workableTiles;
}

void WorkerAssignmentManager::AutoAssignWorkers()
{
    auto availableTiles = GetAvailableTiles_();
    auto prioritizedTiles = PrioritizeAvailableTiles_(availableTiles);
    AutoAssignWorkers_(prioritizedTiles);

    for (Pop* pPop : GetUnassignedWorkers_())
    {
        if (pPop && pPop->IsWorker() && pPop->GetTile() == nullptr)
        {
            ConvertToFallback_(*pPop);
        }
    }
}

void WorkerAssignmentManager::UserAssignBestAvailableWorker(const Tile* pTile, const std::string& defaultWorkerType)
{
    for (int i = static_cast<int>(m_rPops.GetPops().size()) - 1; i >= 0; --i)
    {
        Pop* pPop = m_rPops.GetPops()[i].get();
        if (pPop->IsWorker() && pPop->GetTile() == nullptr)
        {
            UserAssignWorker(*pPop, pTile);
            return;
        }
    }

    for (int i = static_cast<int>(m_rPops.GetPops().size()) - 1; i >= 0; --i)
    {
        Pop* pPop = m_rPops.GetPops()[i].get();
        if (pPop->IsSpecialist())
        {
            m_rPops.ConvertTo(*pPop, defaultWorkerType);
            UserAssignWorker(*pPop, pTile);
            return;
        }
    }

    if (m_pLastAssigned && m_pLastAssigned->IsWorker())
    {
        UnassignWorker(*m_pLastAssigned);
        UserAssignWorker(*m_pLastAssigned, pTile);
    }
}

std::vector<Pop*> WorkerAssignmentManager::GetUnassignedWorkers_() const
{
    std::vector<Pop*> unassignedWorkers;
    for (const auto& rPop : m_rPops.GetPops())
    {
        if (!rPop->IsWorker())
        {
            continue;
        }
        const Tile* pTile = rPop->GetTile();
        if (IsUnassignedTile(pTile) && !rPop->IsUserAssigned())
        {
            unassignedWorkers.push_back(rPop.get());
        }
    }
    return unassignedWorkers;
}

std::vector<const Tile*> WorkerAssignmentManager::GetAvailableTiles_() const
{
    std::vector<const Tile*> availableTiles;
    for (const Tile* pTile : m_workableTiles)
    {
        if (pTile && !IsTileAssigned(pTile))
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

void WorkerAssignmentManager::AutoAssignWorkers_(std::vector<const Tile*>& availableTiles)
{
    for (const auto& rPop : m_rPops.GetPops())
    {
        if (!rPop || !rPop->IsWorker() || rPop->IsUserAssigned())
        {
            continue;
        }
        if (availableTiles.empty())
        {
            break;
        }
        AssignWorker(*rPop, availableTiles[0]);
        availableTiles.erase(availableTiles.begin());
    }
}

void WorkerAssignmentManager::UnassignFromTile_(Pop& rPop)
{
    rPop.SetTile(nullptr);
    ConvertToFallback_(rPop);
}

void WorkerAssignmentManager::ConvertToFallback_(Pop& rPop)
{
    m_rPops.ConvertToFallback(rPop);
}


} // namespace ac

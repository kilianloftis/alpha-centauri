#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/Tile.h"
#include "lib/effects/TileEffectsContext.h"
#include <algorithm>
#include <limits>
#include <ranges>

namespace ac
{

namespace
{

constexpr bool IsUnassignedTile(const Tile* pTile)
{
    return pTile == nullptr;
}

} // namespace

WorkerAssignmentManager::WorkerAssignmentManager(std::vector<const Tile*> workableTiles, PopulationManager& rPops,
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
    return true;
}

void WorkerAssignmentManager::UserUnassignTile(const Tile* pTile)
{
    for (Pop& rPop : m_rPops.Pops())
    {
        if (rPop.IsWorker() && rPop.GetTile() == pTile)
        {
            UnassignFromTile_(rPop);
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
    for (Pop& rPop : m_rPops.Pops())
    {
        if (rPop.IsWorker() && rPop.IsUserAssigned())
        {
            ReleaseUserAssignment(rPop);
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
    for (Pop& rPop : m_rPops.Pops())
    {
        const Tile* pTile = rPop.GetTile();
        if (pTile && !rPop.IsUserAssigned())
        {
            rPop.SetTile(nullptr);
        }
    }
}

void WorkerAssignmentManager::ResetAllAssignments()
{
    for (Pop& rPop : m_rPops.Pops())
    {
        UnassignWorker(rPop);
    }

    AutoAssignWorkers();
}

bool WorkerAssignmentManager::IsTileAssigned(const Tile* pTile) const
{
    // The worked flag is the single source of truth, maintained by Pop::SetTile. Reading it
    // (rather than scanning this base's pops) also prevents two adjacent bases from both
    // working a shared tile, and is the hook a future enemy unit would clear to free the tile.
    return pTile && pTile->IsWorked();
}

TileResources_t WorkerAssignmentManager::ComputeWorkedResources(const BaseEffects_t& rBaseEffects) const
{
    TileResources_t total{0, 0, 0};
    for (const Pop& rPop : m_rPops.Pops())
    {
        if (!rPop.IsWorker())
        {
            continue;
        }

        const Tile* pTile = rPop.GetTile();
        if (!pTile)
        {
            continue;
        }

        const TileResources_t yield = m_rTileEffects.ResolveTileYield(*pTile, /*isBaseTile*/false, rBaseEffects);
        const TileResources_t modified = rPop.ApplyTileMultipliers(yield);

        total.nutrients += modified.nutrients;
        total.energy    += modified.energy;
        total.minerals  += modified.minerals;
    }
    return total;
}

TileResources_t WorkerAssignmentManager::GetWorkedTileYield(const Tile& rTile,
                                                            const BaseEffects_t& rBaseEffects) const
{
    for (const Pop& rPop : m_rPops.Pops())
    {
        if (!rPop.IsWorker() || rPop.GetTile() != &rTile)
        {
            continue;
        }

        const TileResources_t yield = m_rTileEffects.ResolveTileYield(rTile, /*isBaseTile*/false, rBaseEffects);
        return rPop.ApplyTileMultipliers(yield);
    }
    return TileResources_t{0, 0, 0};
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
    // Reverse order: prefer the most recently added pop when several are eligible.
    for (Pop& rPop : std::views::reverse(m_rPops.Pops()))
    {
        if (rPop.IsWorker() && rPop.GetTile() == nullptr)
        {
            UserAssignWorker(rPop, pTile);
            return;
        }
    }

    for (Pop& rPop : std::views::reverse(m_rPops.Pops()))
    {
        if (rPop.IsSpecialist())
        {
            m_rPops.ConvertTo(rPop, defaultWorkerType);
            UserAssignWorker(rPop, pTile);
            return;
        }
    }

    Pop* pWorker = FindLowestYieldAssignedWorker_();
    if (pWorker)
    {
        UnassignWorker(*pWorker);
        UserAssignWorker(*pWorker, pTile);
    }
}

Pop* WorkerAssignmentManager::FindLowestYieldAssignedWorker_() const
{
    Pop* pBestAuto = nullptr;
    Pop* pBestOverall = nullptr;
    float bestAutoScore = std::numeric_limits<float>::infinity();
    float bestOverallScore = std::numeric_limits<float>::infinity();

    for (Pop& rPop : m_rPops.Pops())
    {
        if (!rPop.IsWorker())
        {
            continue;
        }
        const Tile* pTile = rPop.GetTile();
        if (!pTile)
        {
            continue;
        }
        const float score = m_scorer(*pTile);
        if (!rPop.IsUserAssigned() && score < bestAutoScore)
        {
            bestAutoScore = score;
            pBestAuto = &rPop;
        }
        if (score < bestOverallScore)
        {
            bestOverallScore = score;
            pBestOverall = &rPop;
        }
    }

    return pBestAuto ? pBestAuto : pBestOverall;
}

std::vector<Pop*> WorkerAssignmentManager::GetUnassignedWorkers_() const
{
    std::vector<Pop*> unassignedWorkers;
    for (Pop& rPop : m_rPops.Pops())
    {
        if (!rPop.IsWorker())
        {
            continue;
        }
        const Tile* pTile = rPop.GetTile();
        if (IsUnassignedTile(pTile) && !rPop.IsUserAssigned())
        {
            unassignedWorkers.push_back(&rPop);
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
    for (Pop& rPop : m_rPops.Pops())
    {
        if (!rPop.IsWorker() || rPop.IsUserAssigned())
        {
            continue;
        }
        if (availableTiles.empty())
        {
            break;
        }
        AssignWorker(rPop, availableTiles[0]);
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

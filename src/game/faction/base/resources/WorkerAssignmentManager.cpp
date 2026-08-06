#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/Tile.h"
#include "game/map/WorkedTileIndex.h"
#include "game/effects/TileEffectsContext.h"
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
                                                 const TileEffectsContext& rTileEffects, WorkedTileIndex& rWorkedTiles)
    : m_workableTiles(std::move(workableTiles))
    , m_scorer([this](const Tile& rTile) -> float
      {
          // Intrinsic + area (incl. post-restriction bonuses). Caps need base effects and are
          // applied on the production path; ranking without them is acceptable for auto-assign.
          const TileResources_t yield = m_rTileEffects.ResolveTileYield(rTile).effective;
          return static_cast<float>(yield.nutrients + yield.energy + yield.minerals);
      })
    , m_rPops(rPops)
    , m_rTileEffects(rTileEffects)
    , m_rWorkedTiles(rWorkedTiles)
{
}

bool WorkerAssignmentManager::UserAssignWorker(Pop& rPop, const Tile* pTile)
{
    return Assign_(rPop, pTile, /*bUserAssigned*/true);
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
    rPop.SetTileClaim(WorkedTileClaim{});
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
    return Assign_(rPop, pTile, /*bUserAssigned*/false);
}

bool WorkerAssignmentManager::Assign_(Pop& rPop, const Tile* pTile, bool bUserAssigned)
{
    if (!rPop.IsWorker())
    {
        return false;
    }

    if (std::find(m_workableTiles.begin(), m_workableTiles.end(), pTile) == m_workableTiles.end())
    {
        return false;
    }

    // TryClaim is the atomic check-and-claim: it fails if the tile is worked by anyone,
    // including another base or faction. If a base is later founded on the tile, the
    // handler re-runs auto-assignment so the displaced worker moves to the best free tile.
    WorkedTileClaim claim = m_rWorkedTiles.TryClaim(*pTile, bUserAssigned,
        [this]() { AutoAssignWorkers(); });
    if (!claim.GetTile())
    {
        return false;
    }

    rPop.SetTileClaim(std::move(claim));
    return true;
}

void WorkerAssignmentManager::UnassignWorker(Pop& rPop)
{
    rPop.SetTileClaim(WorkedTileClaim{});
}

void WorkerAssignmentManager::UnassignAll()
{
    for (Pop& rPop : m_rPops.Pops())
    {
        const Tile* pTile = rPop.GetTile();
        if (pTile && !rPop.IsUserAssigned())
        {
            rPop.SetTileClaim(WorkedTileClaim{});
        }
    }
}

void WorkerAssignmentManager::ResetAllAssignments()
{
    PopulationManager::BatchCompositionUpdate batch(m_rPops);

    for (Pop& rPop : m_rPops.Pops())
    {
        UnassignWorker(rPop);
    }

    // Pull specialists back into the worker pool so free tiles are filled before any
    // overflow is promoted to the fallback specialist again.
    for (Pop& rPop : m_rPops.Pops())
    {
        if (rPop.IsSpecialist())
        {
            m_rPops.ConvertToDefaultPopType(rPop);
        }
    }

    AutoAssignWorkers();
}

bool WorkerAssignmentManager::IsTileAssigned(const Tile* pTile) const
{
    // WorkedTileIndex is the world-wide single source of truth, so this also reports tiles
    // worked by other bases — including other factions' — and is where a future mechanic
    // (e.g. an enemy unit occupying the tile) would revoke assignments.
    return pTile && m_rWorkedTiles.IsWorked(*pTile);
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

        const TileResources_t yield = m_rTileEffects.ResolveTileYield(
            *pTile, /*bIsBaseTile*/false, rBaseEffects).effective;
        const TileResources_t modified = rPop.ApplyTileMultipliers(yield);

        total.nutrients += modified.nutrients;
        total.energy    += modified.energy;
        total.minerals  += modified.minerals;
    }
    return total;
}

TileYieldView_t WorkerAssignmentManager::GetWorkedTileYield(
    const Tile& rTile, const BaseEffects_t& rBaseEffects) const
{
    for (const Pop& rPop : m_rPops.Pops())
    {
        if (!rPop.IsWorker() || rPop.GetTile() != &rTile)
        {
            continue;
        }

        const TileYieldView_t yield = m_rTileEffects.ResolveTileYield(
            rTile, /*bIsBaseTile*/false, rBaseEffects);
        return TileYieldView_t{
            rPop.ApplyTileMultipliers(yield.effective),
            rPop.ApplyTileMultipliers(yield.potential),
        };
    }
    return TileYieldView_t{};
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
    PopulationManager::BatchCompositionUpdate batch(m_rPops);

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

void WorkerAssignmentManager::UserAssignBestAvailableWorker(const Tile* pTile)
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
            m_rPops.ConvertToDefaultPopType(rPop);
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
    rPop.SetTileClaim(WorkedTileClaim{});
    ConvertToFallback_(rPop);
}

void WorkerAssignmentManager::ConvertToFallback_(Pop& rPop)
{
    m_rPops.ConvertToFallback(rPop);
}


} // namespace ac

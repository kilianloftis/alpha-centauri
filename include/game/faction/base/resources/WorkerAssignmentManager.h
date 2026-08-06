#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/population/pop-types/Pop.h"
#include "game/effects/ActiveEffect.h"
#include <functional>
#include <vector>

namespace ac
{

class PopulationManager;
class Tile;
class TileEffectsContext;
class WorkedTileIndex;

// Manages the mapping of worker pops to workable tiles for a single base.
// Workers are Pop instances that return true from Pop::IsWorker().
//
// An assignment is a WorkedTileClaim held by the Pop (Pop::GetTile()), minted by the
// world-scoped WorkedTileIndex — the single owner of the one-worker-per-tile rule across
// all bases and factions. WorkerAssignmentManager validates assignments against its
// workable tile set and runs the auto-assignment algorithm. It does NOT own tiles.
// Supply crawlers claim tiles on the Unit itself and are collected via HomeBaseIndex.
class WorkerAssignmentManager
{
public:
    WorkerAssignmentManager(std::vector<const Tile*> workableTiles, PopulationManager& rPops,
                             const TileEffectsContext& rTileEffects, WorkedTileIndex& rWorkedTiles);
    // BaseManager constructs m_pPopulation before m_pWorkerAssignments, so member destruction
    // order tears this manager down first while pops (and their WorkedTileClaims) still live.
    // Assign_ hands each claim a `[this]{ AutoAssignWorkers(); }` displaced-worker handler
    // (see WorkedTileIndex), so a claim surviving past this destructor would hold a dangling
    // `this`. Clear every worker's claim here — before that becomes possible — rather than
    // relying on callers to never trigger displacement during base teardown.
    ~WorkerAssignmentManager();

    using TileScorer = std::function<float(const Tile&)>;

    // Assign a worker pop to a tile.
    // Returns false if:
    //   - the pop is not a worker
    //   - the tile is already worked (by any base of any faction)
    //   - the tile is not in the workable tile set
    bool AssignWorker(Pop& rPop, const Tile* pTile);

    // Assign a worker pop to a tile and mark it as user-assigned.
    // User-assigned pops are not displaced by AutoAssignWorkers.
    // Returns false under the same conditions as AssignWorker.
    bool UserAssignWorker(Pop& rPop, const Tile* pTile);

    // Remove the worker from a tile and revert them to their fallback pop type.
    // Use when the player explicitly removes a worker from a tile and no auto-assign follows.
    void UserUnassignTile(const Tile* pTile);

    // Release a pop from its user assignment without reverting to fallback.
    // The pop remains a worker with its tile cleared, eligible for auto-assignment.
    // Call AutoAssignWorkers() afterward to fill the vacancy.
    void ReleaseUserAssignment(Pop& rPop);

    // Release all user-assigned pops so they are eligible for auto-assignment.
    // Does NOT convert pops to fallback — call AutoAssignWorkers() afterward.
    void ReleaseAllUserAssignments();

    // Remove the tile assignment for the given pop. No-op if not assigned.
    // Does not convert to fallback; use when the pop will be immediately repositioned or converted.
    void UnassignWorker(Pop& rPop);

    // Remove all auto-assigned assignments. Does NOT clear user-assigned pops.
    void UnassignAll();

    // Unassign all pops, revert specialists to workers, and re-run auto-assignment.
    void ResetAllAssignments();

    // Returns true if the given tile already has a worker assigned — by this base or any
    // other, including other factions' (queried from the world-scoped WorkedTileIndex).
    bool IsTileAssigned(const Tile* pTile) const;

    // Compute aggregate resources from all assigned workers.
    // For each worker pop with an assigned tile, resolves the tile's full yield (intrinsic +
    // area effects + any base-wide per-tile modifier in rBaseEffects matching the tile), then
    // applies the pop's tile multipliers so those multipliers scale the whole tile yield.
    // Does NOT include the base center tile or supply crawlers — those are collected by
    // ResourceManager (center tile + HomeBaseIndex units).
    TileResources_t ComputeWorkedResources(const BaseEffects_t& rBaseEffects) const;

    // Full yield from one assigned worker tile: intrinsic + area + matching base-wide per-tile
    // modifiers, then the working pop's tile multipliers. Returns zeros if no worker is on rTile.
    // Production reads .effective; UI may also use .potential.
    TileYieldView_t GetWorkedTileYield(const Tile& rTile, const BaseEffects_t& rBaseEffects) const;

    // Auto-assign any unassigned workers to the stored workable tiles.
    // Tiles are sorted by score (descending) before assignment.
    void AutoAssignWorkers();

    // Set the tile scoring function used to prioritize tiles during AutoAssignWorkers().
    // Default scorer sums all three resource yields. May be replaced at runtime.
    void SetTileScorer(TileScorer scorer);

    // Returns the set of workable tiles this manager can assign.
    const std::vector<const Tile*>& GetWorkableTiles() const;

    void UserAssignBestAvailableWorker(const Tile* pTile);

private:
    bool Assign_(Pop& rPop, const Tile* pTile, bool bUserAssigned);
    std::vector<Pop*> GetUnassignedWorkers_() const;
    std::vector<const Tile*> GetAvailableTiles_() const;
    std::vector<const Tile*> PrioritizeAvailableTiles_(const std::vector<const Tile*>& availableTiles) const;
    void AutoAssignWorkers_(std::vector<const Tile*>& availableTiles);

    void UnassignFromTile_(Pop& rPop);
    void ConvertToFallback_(Pop& rPop);
    Pop* FindLowestYieldAssignedWorker_() const;

    std::vector<const Tile*> m_workableTiles;
    TileScorer m_scorer;
    PopulationManager& m_rPops;
    const TileEffectsContext& m_rTileEffects;
    WorkedTileIndex& m_rWorkedTiles;
};

} // namespace ac

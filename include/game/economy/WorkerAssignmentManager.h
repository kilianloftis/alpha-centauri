#pragma once

#include "game/base/BaseTypes.h"
#include "game/population/Pop.h"
#include "game/effects/ActiveEffect.h"
#include <functional>
#include <vector>

namespace ac
{

class PopulationManager;
class Tile;
class TileEffectsContext;
class WorkedTileIndex;

// Worker-to-tile assignment for one base. Claims come from the world WorkedTileIndex, which
// owns the one-worker rule; this validates and auto-assigns.
class WorkerAssignmentManager
{
public:
    WorkerAssignmentManager(std::vector<const Tile*> workableTiles, PopulationManager& rPops,
                             const TileEffectsContext& rTileEffects, WorkedTileIndex& rWorkedTiles);
    // Clears every claim first: their displacement handlers capture `this`.
    ~WorkerAssignmentManager();

    using TileScorer = std::function<float(const Tile&)>;

    // False if the pop is not a worker, the tile is worked, or it is not workable here.
    bool AssignWorker(Pop& rPop, const Tile* pTile);

    // As AssignWorker, user-assigned, so auto-assignment does not displace it.
    bool UserAssignWorker(Pop& rPop, const Tile* pTile);

    // Remove the worker from a tile and revert them to their fallback pop type.
    // Use when the player explicitly removes a worker from a tile and no auto-assign follows.
    void UserUnassignTile(const Tile* pTile);


    // Remove the tile assignment for the given pop. No-op if not assigned.
    // Does not convert to fallback; use when the pop will be immediately repositioned or converted.
    void UnassignWorker(Pop& rPop);

    // Remove all auto-assigned assignments. Does NOT clear user-assigned pops.
    void UnassignAll();

    // Unassign all pops, revert specialists to workers, and re-run auto-assignment.
    void ResetAllAssignments();

    // Whether anyone works the tile; for this base only, IsTileWorkedByThisBase.
    bool IsTileAssigned(const Tile* pTile) const;

    // Whether one of this base's pops works the tile; differs from IsTileAssigned where radii
    // overlap.
    bool IsTileWorkedByThisBase(const Tile* pTile) const;

    // Every assigned worker's full tile yield times its multipliers. Excludes the base tile and
    // supply crawlers.
    TileResources_t ComputeWorkedResources(const BaseEffects_t& rBaseEffects) const;

    // One worker tile's yield times the pop's multipliers; zeros if nobody works it.
    TileYieldView_t GetWorkedTileYield(const Tile& rTile, const BaseEffects_t& rBaseEffects) const;

    // Auto-assign any unassigned workers to the stored workable tiles.
    // Tiles are sorted by score (descending) before assignment.
    void AutoAssignWorkers();

    // Set the tile scoring function used to prioritize tiles during AutoAssignWorkers().
    // Default scorer sums all three resource yields. May be replaced at runtime.
    void SetTileScorer(TileScorer scorer);

    // Returns the set of workable tiles this manager can assign.
    const std::vector<const Tile*>& GetWorkableTiles() const;

    // Puts a pop on pTile: an idle worker, then a specialist, then the weakest worker. False,
    // changing nothing, if the tile is not workable here or is taken.
    bool UserAssignBestAvailableWorker(const Tile* pTile);

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

#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/population/pop-types/Pop.h"
#include <functional>
#include <vector>

namespace ac
{

class PopContainer;
class Tile;

// Manages the mapping of worker pops to workable tiles for a single base.
// Workers are Pop instances that return true from Pop::IsWorker().
//
// The canonical assignment is stored on each Pop as a const Tile* (Pop::GetTile()).
// WorkerAssignmentManager validates assignments, owns the set of workable tiles,
// and runs the auto-assignment algorithm. It does NOT own tiles.
class WorkerAssignmentManager
{
public:
    explicit WorkerAssignmentManager(std::vector<const Tile*> workableTiles, PopContainer& rPops);
    ~WorkerAssignmentManager() = default;

    using TileScorer = std::function<float(const Tile&)>;

    // Assign a worker pop to a tile.
    // Returns false if:
    //   - the pop is not a worker
    //   - the tile is already assigned to another pop
    //   - the tile is not in the workable tile set
    bool AssignWorker(Pop& rPop, const Tile* pTile);

    // Assign a worker pop to a tile and mark it as user-assigned.
    // User-assigned pops are not displaced by AutoAssignWorkers.
    // Returns false under the same conditions as AssignWorker.
    bool UserAssignWorker(Pop& rPop, const Tile* pTile);

    // Clear the user assignment for a given tile.
    // If a pop is currently assigned to that tile, it is also unassigned.
    void UserUnassignTile(const Tile* pTile);

    // Clear all user assignments and unassign the associated worker pops.
    void UserUnassignAll();

    // Remove the assignment for the given pop. No-op if not assigned.
    void UnassignWorker(Pop& rPop);

    // Remove all auto-assigned assignments. Does NOT clear user-assigned pops.
    void UnassignAll();

    // Unassign all pops, revert specialists to workers, and re-run auto-assignment.
    void ResetAllAssignments(const std::string& defaultWorkerTypeId);

    // Returns true if the given tile already has a worker assigned.
    bool IsTileAssigned(const Tile* pTile) const;

    // Compute aggregate resources from all assigned workers.
    // For each worker pop with an assigned tile, reads raw resources from the tile,
    // then applies the pop's tile multipliers.
    // Pops or tiles that cannot be resolved are skipped.
    TileResources_t ComputeWorkedResources() const;

    // Auto-assign any unassigned workers to the stored workable tiles.
    // Tiles are sorted by score (descending) before assignment.
    void AutoAssignWorkers();

    // Set the tile scoring function used to prioritize tiles during AutoAssignWorkers().
    // Default scorer sums all three resource yields. May be replaced at runtime.
    void SetTileScorer(TileScorer scorer);

    // Returns the set of workable tiles this manager can assign.
    const std::vector<const Tile*>& GetWorkableTiles() const;

private:
    std::vector<Pop*> GetUnassignedWorkers_() const;
    std::vector<const Tile*> GetAvailableTiles_() const;
    std::vector<const Tile*> PrioritizeAvailableTiles_(const std::vector<const Tile*>& availableTiles) const;
    void AutoAssignWorkers_(std::vector<const Tile*>& availableTiles);

    void ConvertToSpecialist_(Pop& rPop);


    std::vector<const Tile*> m_workableTiles;
    TileScorer m_scorer;
    PopContainer& m_rPops;
};

} // namespace ac

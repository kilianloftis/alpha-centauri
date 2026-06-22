#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/population/pop-types/Pop.h"
#include <functional>
#include <vector>

namespace ac
{

class PopContainer;
class Tile;

// Manages the mapping of worker pops to workable tile coordinates for a single base.
// Workers are Pop instances that return true from Pop::IsWorker().
// Tile coordinates are (x, y) pairs matching BaseManager::GetWorkableTilePositions().
//
// The canonical assignment is stored on each Pop (Pop::GetTileCoord()).
// WorkerAssignmentManager validates assignments, owns the set of workable tiles,
// and runs the auto-assignment algorithm. It does NOT own tiles.
class WorkerAssignmentManager
{
public:
    explicit WorkerAssignmentManager(std::vector<const Tile*> workableTiles);
    ~WorkerAssignmentManager() = default;

    using TileLookup = std::function<const Tile*(int x, int y)>;
    using TileScorer = std::function<float(const Tile&)>;

    // Assign a worker pop to a tile coordinate.
    // Returns false if:
    //   - the pop is not a worker
    //   - the tile coordinate is already assigned to another pop
    //   - the tile coordinate is not in the workable tile set
    bool AssignWorker(Pop& rPop, int x, int y, PopContainer& rPops);

    // Remove the assignment for the given pop. No-op if not assigned.
    void UnassignWorker(Pop& rPop);

    // Remove all assignments.
    void UnassignAll(PopContainer& rPops);

    // Returns true if the given tile coordinate already has a worker assigned.
    bool IsTileAssigned(int x, int y, const PopContainer& rPops) const;

    // Compute aggregate resources from all assigned workers.
    // For each worker pop with a tile coordinate, looks up the tile via tileAt(x, y),
    // reads raw resources, then applies the pop's tile multipliers.
    // Pops or tiles that cannot be resolved are skipped.
    TileResources_t ComputeWorkedResources(const PopContainer& rPops,
                                           const TileLookup& tileAt) const;

    // Update the set of workable tiles. Should be called when the base position changes.
    // Any pop assigned to a tile no longer in the workable set is unassigned.
    void SetWorkableTiles(std::vector<const Tile*> workableTiles, PopContainer& rPops);

    // Auto-assign any unassigned workers to the stored workable tiles.
    // Tiles are sorted by score (descending) before assignment.
    void AutoAssignWorkers(PopContainer& rPops);

    // Set the tile scoring function used to prioritize tiles during AutoAssignWorkers().
    // Default scorer sums all three resource yields. May be replaced at runtime.
    void SetTileScorer(TileScorer scorer);

    // Returns the set of workable tiles this manager can assign.
    const std::vector<const Tile*>& GetWorkableTiles() const;

private:
    std::vector<Pop*> GetUnassignedWorkers_(const PopContainer& rPops) const;
    std::vector<const Tile*> GetAvailableTiles_(const PopContainer& rPops) const;
    std::vector<const Tile*> PrioritizeAvailableTiles_(const std::vector<const Tile*>& availableTiles) const;
    void AutoAssignWorkers_(const std::vector<Pop*>& unassignedWorkers,
                            const std::vector<const Tile*>& availableTiles,
                            PopContainer& rPops);

    bool IsTileAssigned_(int x, int y, const PopContainer& rPops) const;
    bool IsTileWorkable_(int x, int y) const;

    std::vector<const Tile*> m_workableTiles;
    TileScorer m_scorer;
};

} // namespace ac

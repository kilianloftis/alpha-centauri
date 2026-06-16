#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/population/pop-types/Pop.h"
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ac
{

class PopContainer;
class Tile;

// Manages the mapping of worker pops to workable tile coordinates for a single base.
// Workers are identified by stable pop IDs (set by PopContainer at creation).
// Tile coordinates are (x, y) pairs matching BaseManager::GetWorkableTilePositions().
//
// This class does NOT own tiles. The set of workable tiles is provided at construction
// and can be updated via SetWorkableTiles() when the base position changes.
class WorkerAssignmentManager
{
public:
    explicit WorkerAssignmentManager(std::vector<const Tile*> workableTiles);
    ~WorkerAssignmentManager() = default;

    using TileCoord = std::pair<int, int>;
    using TileLookup = std::function<const Tile*(int x, int y)>;
    using TileScorer = std::function<float(const Tile&)>;

    // Assign a worker pop to a tile coordinate.
    // Returns false if:
    //   - the pop does not exist or is not a worker
    //   - the tile coordinate is already assigned to another pop
    bool AssignWorker(int popId, int x, int y, const PopContainer& rPops);

    // Remove the assignment for the given pop ID. No-op if not assigned.
    void UnassignWorker(int popId);

    // Remove all assignments.
    void UnassignAll();

    // Returns true if the given tile coordinate already has a worker assigned.
    bool IsTileAssigned(int x, int y) const;

    // Returns the tile coordinate assigned to a pop, or {-1,-1} if unassigned.
    TileCoord GetAssignedTile(int popId) const;

    // Returns all current pop ID -> tile coord assignments.
    const std::unordered_map<int, TileCoord>& GetAssignments() const;

    // Compute aggregate resources from all assigned workers.
    // For each assignment, looks up the tile via tileAt(x, y), reads raw resources,
    // then applies the pop's tile multipliers via Pop::ApplyTileMultipliers().
    // Pops or tiles that cannot be resolved are skipped.
    TileResources_t ComputeWorkedResources(const PopContainer& rPops,
                                           const TileLookup& tileAt) const;

    // Update the set of workable tiles. Should be called when the base position changes.
    void SetWorkableTiles(std::vector<const Tile*> workableTiles);

    // Auto-assign any unassigned workers to the stored workable tiles.
    // Tiles are sorted by score (descending) before assignment.
    void AutoAssignWorkers(const PopContainer& rPops);

    // Set the tile scoring function used to prioritize tiles during AutoAssignWorkers().
    // Default scorer sums all three resource yields. May be replaced at runtime.
    void SetTileScorer(TileScorer scorer);

private:
    std::vector<int> GetUnassignedWorkers_(const PopContainer& rPops) const;
    std::vector<const Tile*> GetAvailableTiles_() const;
    std::vector<const Tile*> PrioritizeAvailableTiles_(const std::vector<const Tile*>& availableTiles) const;
    void AutoAssignWorkers_(const std::vector<int>& unassignedWorkerIds,
                            const std::vector<const Tile*>& availableTiles,
                            const PopContainer& rPops);

    std::unordered_map<int, TileCoord> m_assignments;  // popId -> (x, y)
    std::vector<const Tile*> m_workableTiles;
    TileScorer m_scorer;

    const Pop* FindPop_(int popId, const PopContainer& rPops) const;
};

} // namespace ac

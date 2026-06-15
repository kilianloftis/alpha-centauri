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
// This class does NOT own tiles. Resource computation requires a tile-lookup
// callable supplied at call-site; this decouples the manager from the future TileMap.
class WorkerAssignmentManager
{
public:
    WorkerAssignmentManager();
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

    // Auto-assign any unassigned workers to available workable tiles.
    // Tiles are sorted by score (descending) before assignment if a TileLookup
    // has been set via SetTileLookup(). Otherwise, order is unspecified.
    void AutoAssignWorkers(const PopContainer& rPops,
                           const std::vector<TileCoord>& workableTiles);

    // Set the tile lookup used to resolve coordinates during AutoAssignWorkers().
    // If not set, tile scoring is skipped and assignment order is unspecified.
    void SetTileLookup(TileLookup tileLookup);

    // Set the tile scoring function used to prioritize tiles during AutoAssignWorkers().
    // Default scorer sums all three resource yields. May be replaced at runtime.
    void SetTileScorer(TileScorer scorer);

private:
    std::vector<int> GetUnassignedWorkers_(const PopContainer& rPops) const;
    std::vector<TileCoord> GetAvailableTiles_(const std::vector<TileCoord>& workableTiles) const;
    std::vector<TileCoord> PrioritizeAvailableTiles_(const std::vector<TileCoord>& availableTiles) const;
    void AutoAssignWorkers_(const std::vector<int>& unassignedWorkerIds,
                            const std::vector<TileCoord>& availableTiles,
                            const PopContainer& rPops);

    std::unordered_map<int, TileCoord> m_assignments;  // popId -> (x, y)
    TileLookup m_tileLookup;
    TileScorer m_scorer;

    const Pop* FindPop_(int popId, const PopContainer& rPops) const;
};

} // namespace ac

#pragma once

#include "game/faction/base/BaseTypes.h"
#include <memory>
#include <string>
#include <vector>

namespace ac
{

// Forward declarations
class PopulationManager;
class WorkerAssignmentManager;
class ResourceManager;

// BaseManager coordinates base management subsystems.
// Provides identity, position, and access to sub-managers.
// Routes API calls to PopulationManager, ResourceManager, and WorkerAssignmentManager.
class BaseManager
{
public:
    BaseManager();
    ~BaseManager();

    // Population management - delegated to PopulationManager
    PopulationManager* GetPopulation();
    const PopulationManager* GetPopulation() const;
    void AddPop();

    // Worker assignment - delegated to WorkerAssignmentManager
    WorkerAssignmentManager& GetWorkerAssignments();
    const WorkerAssignmentManager& GetWorkerAssignments() const;

    // Auto-assign all unassigned workers to available workable tiles.
    // Should be called after initial population setup or when new workers need assignment.
    void AutoAssignWorkers();

    // Resource management - delegated to ResourceManager
    ResourceManager* GetResourceManager();
    const ResourceManager* GetResourceManager() const;

    // Collect resources from worked tiles and allocate energy to stockpiles.
    // Called once per turn per base during ResourceCollection stage.
    void CollectResources(class BaseEconomyManager* pEconomy);

    // Position on the map
    void SetPosition(int x, int y);
    int GetX() const;
    int GetY() const;

    // Returns the set of (x,y) tile coordinates this base can assign workers to.
    // Produces a 5x5 grid with the four corners removed (Manhattan distance <= 3
    // within the [-2,2] bounding box), excluding the base's own tile.
    // Does not filter for enemy units (TODO: when units exist).
    std::vector<std::pair<int, int>> GetWorkableTilePositions() const;

    // Base identity
    void SetName(const std::string& name);
    const std::string& GetName() const;

    // Ownership
    void SetFactionId(FactionId factionId);
    FactionId GetFactionId() const;
    void SetBaseId(int baseId);
    int GetBaseId() const;

protected:
    FactionId m_factionId;
    int m_baseId;
    int m_x;
    int m_y;
    std::unique_ptr<PopulationManager> m_pPopulation;
    std::unique_ptr<WorkerAssignmentManager> m_pWorkerAssignments;
    std::unique_ptr<ResourceManager> m_pResources;
    std::string m_name;
};

} // namespace ac

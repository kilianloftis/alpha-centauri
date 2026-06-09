#pragma once

#include "game/faction/base/BaseTypes.h"
#include <memory>
#include <string>

namespace ac
{

// Forward declarations
class PopulationManager;
class WorkerAssignmentManager;
class ResourceManager;

// Thin interface class for base management.
// Provides identity, position, and access to sub-managers.
// Resource management functionality is delegated to ResourceManager subclass.
class BaseManager
{
public:
    BaseManager();
    virtual ~BaseManager();

    // Population management
    PopulationManager* GetPopulation();
    const PopulationManager* GetPopulation() const;
    void AddPop();

    // Worker assignment subcomponent
    WorkerAssignmentManager& GetWorkerAssignments();
    const WorkerAssignmentManager& GetWorkerAssignments() const;

    // Resource management subcomponent (may be null if not using ResourceManager)
    virtual ResourceManager* GetResourceManager();
    virtual const ResourceManager* GetResourceManager() const;

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
    std::string m_name;
};

} // namespace ac

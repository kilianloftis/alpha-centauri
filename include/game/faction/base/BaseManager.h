#pragma once

#include "game/faction/base/BaseTypes.h"
#include "lib/Signal.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ac
{

// Forward declarations
class Pop;
class PopulationManager;
class PopContainer;
class PopTypeRegistry;
class PopCompositionCalculator;
class WorkerAssignmentManager;
class ResourceManager;
class BuildingManager;
class BuildingRegistry;
class Tile;
class WorldMap;

// BaseManager coordinates base management subsystems.
// Provides identity, position, and access to sub-managers.
// Routes API calls to PopulationManager, ResourceManager, and WorkerAssignmentManager.
class BaseManager
{
public:
    BaseManager(const BuildingRegistry* pBuildingRegistry, const PopTypeRegistry* pPopRegistry, PopCompositionCalculator* pCompositionCalculator, const WorldMap& rWorldMap);
    ~BaseManager();

    // Population management - delegated to PopulationManager
    void RecalculatePopComposition();
    const PopContainer& GetPopContainer() const;
    PopContainer& GetPopContainer();
    int GetPopWorkerCount() const;
    void AddPop();
    void RemovePop();
    void ConvertPop(Pop& rPop, const std::string& typeId);

    // Signals forwarded from PopulationManager
    Signal<int> on_pop_gained;
    Signal<int> on_pop_lost;

    // Worker assignment - delegated to WorkerAssignmentManager
    WorkerAssignmentManager& GetWorkerAssignments();
    const WorkerAssignmentManager& GetWorkerAssignments() const;

    // Auto-assign all unassigned workers to available workable tiles.
    // Should be called after initial population setup or when new workers need assignment.
    void AutoAssignWorkers();

    // Resource management - delegated to ResourceManager
    int GetNutrientProduction() const;
    int GetMineralProduction() const;
    int GetEnergyProduction() const;
    int GetMineralStockpile() const;

    // Building management - delegated to BuildingManager
    void AddBuilding(const std::string& buildingId);
    void DestroyBuilding(const std::string& buildingId);

    // Collect resources from worked tiles and allocate energy to stockpiles.
    // Called once per turn per base during ResourceCollection stage.
    void CollectResources(class BaseEconomyManager* pEconomy);

    // Returns the accumulated econ stockpile and resets it to 0.
    // Called during IncomeCollection stage to transfer income to the faction.
    int CollectIncome();

    // Returns the accumulated labs stockpile and resets it to 0.
    // Called during ResearchAccumulation stage to transfer research to the faction.
    int CollectLabs();

    // Convenience accessors for Population stage
    int GetNutrientStockpile() const;
    void SetNutrientStockpile(int amount);
    int GetBaseSize() const;
    int GetGrowthRate() const;

    // Position on the map
    void SetPosition(int x, int y);
    int GetX() const;
    int GetY() const;

    // Returns the set of workable tiles this base can assign workers to.
    // Produces a 5x5 grid with the four corners removed (Manhattan distance <= 3
    // within the [-2,2] bounding box), excluding the base's own tile.
    // Does not filter for enemy units (TODO: when units exist).
    std::vector<const Tile*> GetWorkableTilePositions() const;

    // Base identity
    void SetName(const std::string& name);
    const std::string& GetName() const;

    // Ownership
    void SetFactionId(FactionId factionId);
    FactionId GetFactionId() const;
    void SetBaseId(int baseId);
    int GetBaseId() const;

private:
    FactionId m_factionId;
    int m_baseId;
    int m_x;
    int m_y;
    const WorldMap* m_pWorldMap;
    std::unique_ptr<PopulationManager> m_pPopulation;
    std::unique_ptr<WorkerAssignmentManager> m_pWorkerAssignments;
    std::unique_ptr<ResourceManager> m_pResources;
    std::unique_ptr<BuildingManager> m_pBuildings;
    std::string m_name;
};

} // namespace ac

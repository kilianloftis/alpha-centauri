#pragma once

#include "game/IConstructable.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/faction/base/BaseTypes.h"
#include "lib/effects/ActiveEffect.h"
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
class EconomyManager;
class ResourceManager;
class BuildingManager;
class BuildingRegistry;
class GrowthCalculator;
class SecretProjectAvailabilityCalculator;
class PopTypeAvailabilityCalculator;
class ProductionCostCalculator;
class ProductionManager;
class ResearchManager;
class Tile;
class WorldMap;
class ImprovementRegistry;

// BaseManager coordinates base management subsystems.
// Provides identity, position, and access to sub-managers.
// Routes API calls to PopulationManager, ResourceManager, and WorkerAssignmentManager.
class BaseManager
{
public:
    BaseManager(
        Tile& tile,
        const BuildingRegistry* pBuildingRegistry,
        const PopTypeRegistry* pPopRegistry,
        const PopTypeAvailabilityCalculator* pPopTypeAvailabilityCalculator,
        PopCompositionCalculator* pCompositionCalculator,
        const WorldMap& rWorldMap,
        const ResearchManager* pResearchManager,
        const EconomyManager* pEconomyManager,
        const ProductionCostCalculator* pProductionCostCalculator,
        const GrowthCalculator* pGrowthCalculator,
        const SecretProjectAvailabilityCalculator* pSecretProjectCalculator,
        const ImprovementRegistry* pImprovementRegistry);
    ~BaseManager();

    // Population management - delegated to PopulationManager
    void RecalculatePopComposition();
    const PopContainer& GetPopContainer() const;
    int GetPopWorkerCount() const;
    void ConvertPop(Pop& rPop, const std::string& typeId);
    const std::string& GetDefaultWorkerTypeId() const;

    // Signals forwarded from PopulationManager
    Signal<int> on_pop_gained;
    Signal<int> on_pop_lost;

    // Signals forwarded from ProductionManager
    Signal<std::string> on_production_completed;

    // Worker assignment - delegated to WorkerAssignmentManager
    WorkerAssignmentManager& GetWorkerAssignments();
    const WorkerAssignmentManager& GetWorkerAssignments() const;

    // Find the best available pop and user-assign them to the given tile.
    // Priority: unassigned worker → specialist converted to worker → steal an assigned worker.
    // No-op if the tile is not workable or no pop can be found.
    void UserAssignBestAvailableWorker(const Tile* pTile);

    // Auto-assign all unassigned workers to available workable tiles.
    // Should be called after initial population setup or when new workers need assignment.
    void AutoAssignWorkers();

    // Resource production per turn (calculated live).
    int GetNutrientProduction() const;
    int GetMineralProduction() const;
    int GetEconProduction() const;
    int GetLabsProduction() const;
    int GetPsychProduction() const;

    // Consume the full accumulated resource stockpile, returning the amount consumed.
    int ConsumeEcon();
    int ConsumeLabs();
    int ConsumePsych();

    // Building management - delegated to BuildingManager
    void AddBuilding(const std::string& buildingId);
    void DestroyBuilding(const std::string& buildingId);
    const std::vector<const BuildingConfig_t*>& GetBuildings() const;
    std::vector<const IConstructable*> GetConstructable() const;

    // Production management - delegated to ProductionManager
    void SetProduction(const IConstructable* pItem);
    const IConstructable* GetCurrentProduction() const;
    int GetProductionMineralCost() const;
    int GetMineralStockpile() const;

    // Collect minerals from ResourceManager and apply to production this turn.
    // Completes construction if the stockpile meets the cost.
    // Returns the completed item id, or empty string if construction is ongoing.
    std::string ApplyProduction();

    // Collect resources from worked tiles and allocate energy to categories.
    // Called once per turn per base during ResourceCollection stage.
    void ProduceResources(const std::vector<ActiveEffect_t>& activeEffects);

    // Apply nutrients produced this turn: add to stockpile, grow or starve if threshold is met.
    void ApplyGrowth();

    int GetNutrientStockpile() const;
    int GetNutrientsRequired() const;
    int GetBaseSize() const;
    int GetGrowthRate() const;

    int GetX() const;
    int GetY() const;

    // Returns the set of workable tiles this base can assign workers to.
    const std::vector<const Tile*>& GetWorkableTilePositions() const;

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
    Tile& m_tile;
    const ResearchManager* m_pResearch;
    const ImprovementRegistry* m_pImprovementRegistry;
    std::unique_ptr<PopulationManager> m_pPopulation;
    std::unique_ptr<WorkerAssignmentManager> m_pWorkerAssignments;
    std::unique_ptr<ResourceManager> m_pResources;
    std::unique_ptr<BuildingManager> m_pBuildings;
    std::unique_ptr<ProductionManager> m_pProduction;
    std::string m_name;
};

} // namespace ac

#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/map/WorldMap.h"
#include <cmath>

namespace ac
{

BaseManager::BaseManager(const BuildingRegistry* pBuildingRegistry, const PopTypeRegistry* pPopRegistry, PopCompositionCalculator* pCompositionCalculator, const WorldMap& rWorldMap)
    : m_factionId(-1)
    , m_baseId(-1)
    , m_x(0)
    , m_y(0)
    , m_pWorldMap(&rWorldMap)
    , m_pPopulation(std::make_unique<PopulationManager>(pPopRegistry, pCompositionCalculator))
    , m_pWorkerAssignments(std::make_unique<WorkerAssignmentManager>(std::vector<const Tile*>{}))
    , m_pResources(nullptr)
    , m_pBuildings(std::make_unique<BuildingManager>(pBuildingRegistry))
    , m_pProduction(std::make_unique<ProductionManager>(pBuildingRegistry))
{
    // Create ResourceManager after population and worker assignments are set up
    m_pResources = std::make_unique<ResourceManager>(m_pPopulation.get(), m_pWorkerAssignments.get());
    m_pResources->SetTileLookup([this](int x, int y) -> const Tile* {
        return m_pWorldMap ? m_pWorldMap->GetTile(x, y) : nullptr;
    });

    m_pPopulation->on_pop_gained.connect([this](int newSize) {
        m_pWorkerAssignments->AutoAssignWorkers(m_pPopulation->GetContainer());
        on_pop_gained.emit(newSize);
    });
    // Newly created pops start unassigned; auto-assign once after construction.
    m_pWorkerAssignments->AutoAssignWorkers(m_pPopulation->GetContainer());
    m_pPopulation->on_pop_lost.connect([this](int newSize) {
        on_pop_lost.emit(newSize);
    });

    m_pProduction->on_production_completed.connect([this](const std::string& itemId) {
        m_pBuildings->AddBuilding(itemId);
        on_production_completed.emit(itemId);
    });
}

BaseManager::~BaseManager() = default;

void BaseManager::RecalculatePopComposition()
{
    if (m_pPopulation)
    {
        m_pPopulation->RecalculateComposition();
    }
}

const PopContainer& BaseManager::GetPopContainer() const
{
    return m_pPopulation->GetContainer();
}

PopContainer& BaseManager::GetPopContainer()
{
    return m_pPopulation->GetContainer();
}

int BaseManager::GetPopWorkerCount() const
{
    return m_pPopulation ? m_pPopulation->GetWorkerCount() : 0;
}

void BaseManager::AddPop()
{
    if (m_pPopulation)
    {
        m_pPopulation->AddPop();
    }
}

void BaseManager::RemovePop()
{
    if (m_pPopulation)
    {
        m_pPopulation->RemovePop();
    }
}

void BaseManager::ConvertPop(Pop& rPop, const std::string& typeId)
{
    if (!m_pPopulation)
    {
        return;
    }
    if (rPop.IsWorker())
    {
        m_pWorkerAssignments->UnassignWorker(rPop);
    }
    m_pPopulation->ConvertTo(rPop, typeId);
    if (rPop.IsWorker())
    {
        m_pWorkerAssignments->AutoAssignWorkers(m_pPopulation->GetContainer());
    }
}

WorkerAssignmentManager& BaseManager::GetWorkerAssignments()
{
    return *m_pWorkerAssignments;
}

const WorkerAssignmentManager& BaseManager::GetWorkerAssignments() const
{
    return *m_pWorkerAssignments;
}

void BaseManager::AutoAssignWorkers()
{
    m_pWorkerAssignments->AutoAssignWorkers(m_pPopulation->GetContainer());
}

int BaseManager::GetNutrientProduction() const
{
    return m_pResources ? m_pResources->GetNutrientProduction() : 0;
}

int BaseManager::GetMineralProduction() const
{
    return m_pResources ? m_pResources->GetMineralProduction() : 0;
}

int BaseManager::GetEnergyProduction() const
{
    return m_pResources ? m_pResources->GetEnergyProduction() : 0;
}

int BaseManager::GetMineralStockpile() const
{
    return m_pResources ? m_pResources->GetMineralStockpile() : 0;
}

void BaseManager::AddBuilding(const std::string& buildingId)
{
    if (m_pBuildings)
    {
        m_pBuildings->AddBuilding(buildingId);
    }
}

void BaseManager::DestroyBuilding(const std::string& buildingId)
{
    if (m_pBuildings)
    {
        m_pBuildings->DestroyBuilding(buildingId);
    }
}

std::vector<const BuildingConfig_t*> BaseManager::GetBuildingsAvailableForConstruction(const std::vector<const BuildingConfig_t*>& discoveredBuildings) const
{
    return m_pBuildings ? m_pBuildings->GetBuildingsAvailableForConstruction(discoveredBuildings) : std::vector<const BuildingConfig_t*>{};
}

void BaseManager::SetProduction(const std::string& itemId)
{
    if (m_pProduction)
    {
        m_pProduction->SetProduction(itemId);
    }
}

const std::string& BaseManager::GetProduction() const
{
    static const std::string kEmpty;
    return m_pProduction ? m_pProduction->GetProduction() : kEmpty;
}

int BaseManager::GetProductionMineralCost() const
{
    return m_pProduction ? m_pProduction->GetMineralCost() : 0;
}

std::string BaseManager::CompleteProduction()
{
    return m_pProduction ? m_pProduction->CompleteProduction() : std::string();
}

int BaseManager::ConsumeMinerals(int amount)
{
    return m_pResources ? m_pResources->ConsumeMinerals(amount) : 0;
}

void BaseManager::CollectResources(BaseEconomyManager* pEconomy)
{
    if (m_pResources)
    {
        m_pResources->CollectResources(pEconomy);
    }
}

int BaseManager::CollectIncome()
{
    return m_pResources ? m_pResources->CollectIncome() : 0;
}

int BaseManager::CollectLabs()
{
    return m_pResources ? m_pResources->CollectLabs() : 0;
}

int BaseManager::GetNutrientStockpile() const
{
    return m_pResources ? m_pResources->GetNutrientStockpile() : 0;
}

void BaseManager::SetNutrientStockpile(int amount)
{
    if (m_pResources)
    {
        m_pResources->SetNutrientStockpile(amount);
    }
}

int BaseManager::GetBaseSize() const
{
    return m_pPopulation ? m_pPopulation->GetSize() : 0;
}

int BaseManager::GetGrowthRate() const
{
    // TODO: Implement growth rate calculation
    // For now, return arbitrary value as placeholder
    return 0;
}

void BaseManager::SetPosition(int x, int y)
{
    m_x = x;
    m_y = y;
    m_pWorkerAssignments->SetWorkableTiles(GetWorkableTilePositions(), m_pPopulation->GetContainer());
}

int BaseManager::GetX() const
{
    return m_x;
}

int BaseManager::GetY() const
{
    return m_y;
}

std::vector<const Tile*> BaseManager::GetWorkableTilePositions() const
{
    static constexpr int kGridHalfExtent = 2;   // [-2, 2] bounding box
    static constexpr int kManhattanLimit  = 3;   // excludes corners (|dx|+|dy|==4)
    std::vector<const Tile*> tiles;
    for (int dy = -kGridHalfExtent; dy <= kGridHalfExtent; ++dy)
    {
        for (int dx = -kGridHalfExtent; dx <= kGridHalfExtent; ++dx)
        {
            if (dx == 0 && dy == 0)
            {
                continue;
            }
            if (std::abs(dx) + std::abs(dy) <= kManhattanLimit)
            {
                const Tile* pTile = m_pWorldMap ? m_pWorldMap->GetTile(m_x + dx, m_y + dy) : nullptr;
                if (pTile)
                {
                    tiles.push_back(pTile);
                }
            }
        }
    }
    return tiles;
}

void BaseManager::SetName(const std::string& name)
{
    m_name = name;
}

const std::string& BaseManager::GetName() const
{
    return m_name;
}

void BaseManager::SetFactionId(FactionId factionId)
{
    m_factionId = factionId;
}

FactionId BaseManager::GetFactionId() const
{
    return m_factionId;
}

void BaseManager::SetBaseId(int baseId)
{
    m_baseId = baseId;
}

int BaseManager::GetBaseId() const
{
    return m_baseId;
}

} // namespace ac

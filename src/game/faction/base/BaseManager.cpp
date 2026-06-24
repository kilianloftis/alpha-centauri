#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/base/resources/BaseEconomyManager.h"
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
    , m_pEconomy(std::make_unique<BaseEconomyManager>())
    , m_pResources(nullptr)
    , m_pBuildings(std::make_unique<BuildingManager>(pBuildingRegistry))
    , m_pProduction(std::make_unique<ProductionManager>())
{
    // Create ResourceManager after all sub-managers are set up
    m_pResources = std::make_unique<ResourceManager>(
        m_pPopulation.get(),
        m_pWorkerAssignments.get(),
        m_pEconomy.get(),
        m_pBuildings.get(),
        [this](int x, int y) -> const Tile* {
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

int BaseManager::GetEconProduction() const
{
    return m_pResources ? m_pResources->GetEconProduction() : 0;
}

int BaseManager::GetLabsProduction() const
{
    return m_pResources ? m_pResources->GetLabsProduction() : 0;
}

int BaseManager::GetPsychProduction() const
{
    return m_pResources ? m_pResources->GetPsychProduction() : 0;
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

std::vector<const Building*> BaseManager::GetBuildingsAvailableForConstruction(const std::vector<const Building*>& discoveredBuildings) const
{
    return m_pBuildings ? m_pBuildings->GetBuildingsAvailableForConstruction(discoveredBuildings) : std::vector<const Building*>{};
}

void BaseManager::SetProduction(const Building* pBuilding)
{
    if (m_pProduction)
    {
        m_pProduction->SetProduction(pBuilding);
    }
}

const Building* BaseManager::GetCurrentBuilding() const
{
    return m_pProduction ? m_pProduction->GetCurrentBuilding() : nullptr;
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

int BaseManager::GetMineralStockpile() const
{
    return m_pProduction ? m_pProduction->GetMineralStockpile() : 0;
}

int BaseManager::ConsumeMinerals(int amount)
{
    return m_pProduction ? m_pProduction->ConsumeMinerals(amount) : 0;
}

std::string BaseManager::CompleteProduction()
{
    return m_pProduction ? m_pProduction->CompleteProduction() : std::string();
}

void BaseManager::CollectResources()
{
    if (m_pResources)
    {
        m_pResources->CollectResources();
    }
}

int BaseManager::ConsumeNutrients()
{
    return m_pResources ? m_pResources->ConsumeNutrients() : 0;
}

int BaseManager::ConsumeEcon()
{
    return m_pResources ? m_pResources->ConsumeEcon() : 0;
}

int BaseManager::ConsumeLabs()
{
    return m_pResources ? m_pResources->ConsumeLabs() : 0;
}

int BaseManager::ConsumePsych()
{
    return m_pResources ? m_pResources->ConsumePsych() : 0;
}

int BaseManager::GetNutrientStockpile() const
{
    return m_pPopulation ? m_pPopulation->GetNutrientStockpile() : 0;
}

void BaseManager::SetNutrientStockpile(int amount)
{
    if (m_pPopulation)
    {
        m_pPopulation->SetNutrientStockpile(amount);
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

#include "game/faction/base/BaseManager.h"
#include "game/faction/base/BuildingManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/buildings/BuildingRegistry.h"
#include <cmath>

namespace ac
{

BaseManager::BaseManager(const BuildingRegistry* pBuildingRegistry)
    : m_factionId(-1)
    , m_baseId(-1)
    , m_x(0)
    , m_y(0)
    , m_pPopulation(std::make_unique<PopulationManager>(3))
    , m_pWorkerAssignments(std::make_unique<WorkerAssignmentManager>())
    , m_pResources(nullptr)
    , m_pBuildings(std::make_unique<BuildingManager>(pBuildingRegistry))
{
    // Create ResourceManager after population and worker assignments are set up
    m_pResources = std::make_unique<ResourceManager>(m_pPopulation.get(), m_pWorkerAssignments.get());

    m_pPopulation->on_pop_gained.connect([this](int) {
        m_pWorkerAssignments->OnPopulationChanged(m_pPopulation->GetContainer());
        m_pWorkerAssignments->AutoAssignWorkers(m_pPopulation->GetContainer(),
                                               GetWorkableTilePositions());
    });
    m_pPopulation->on_pop_lost.connect([this](int) {
        m_pWorkerAssignments->OnPopulationChanged(m_pPopulation->GetContainer());
    });
}

BaseManager::~BaseManager() = default;

PopulationManager* BaseManager::GetPopulation()
{
    return m_pPopulation.get();
}

const PopulationManager* BaseManager::GetPopulation() const
{
    return m_pPopulation.get();
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
    m_pWorkerAssignments->AutoAssignWorkers(m_pPopulation->GetContainer(),
                                           GetWorkableTilePositions());
}

void BaseManager::SetTileLookup(std::function<const Tile*(int x, int y)> tileLookup)
{
    if (m_pResources)
    {
        m_pResources->SetTileLookup(std::move(tileLookup));
    }
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

void BaseManager::CollectResources(BaseEconomyManager* pEconomy)
{
    if (m_pResources)
    {
        m_pResources->CollectResources(pEconomy);
    }
}

int BaseManager::GetNutrientStockpile() const
{
    return m_pResources ? m_pResources->GetNutrientStockpile() : 0;
}

void BaseManager::SetNutrientStockpile(int amount)
{
    // TODO: Add setter to ResourceManager
    // For now, this is a placeholder - nutrients are accumulated in CollectResources
    (void)amount;
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
}

int BaseManager::GetX() const
{
    return m_x;
}

int BaseManager::GetY() const
{
    return m_y;
}

std::vector<std::pair<int, int>> BaseManager::GetWorkableTilePositions() const
{
    static constexpr int kGridHalfExtent = 2;   // [-2, 2] bounding box
    static constexpr int kManhattanLimit  = 3;   // excludes corners (|dx|+|dy|==4)
    std::vector<std::pair<int, int>> tiles;
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
                tiles.emplace_back(m_x + dx, m_y + dy);
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

#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/map/Tile.h"
#include <algorithm>
#include <cmath>

namespace ac
{

ResourceManager::ResourceManager(PopulationManager* pPopulation, WorkerAssignmentManager* pWorkerAssignments)
    : m_pPopulation(pPopulation)
    , m_pWorkerAssignments(pWorkerAssignments)
    , m_nutrientStockpile(0)
    , m_mineralStockpile(0)
{
}

ResourceManager::~ResourceManager()
{
}

void ResourceManager::AddBuilding(const std::string& buildingId)
{
    m_buildings.push_back(buildingId);
}

void ResourceManager::RemoveBuilding(const std::string& buildingId)
{
    auto it = std::find(m_buildings.begin(), m_buildings.end(), buildingId);
    if (it != m_buildings.end())
    {
        m_buildings.erase(it);
    }
}

const std::vector<std::string>& ResourceManager::GetBuildings() const
{
    return m_buildings;
}

void ResourceManager::SetTileLookup(std::function<const Tile*(int x, int y)> tileLookup)
{
    m_tileLookup = std::move(tileLookup);
}

void ResourceManager::AddTradeRoute(const TradeRoute_t& tradeRoute)
{
    m_tradeRoutes.push_back(tradeRoute);
}

void ResourceManager::RemoveTradeRoute(int targetFactionId)
{
    auto it = std::remove_if(m_tradeRoutes.begin(), m_tradeRoutes.end(),
        [targetFactionId](const TradeRoute_t& route) {
            return route.targetFactionId == targetFactionId;
        });
    m_tradeRoutes.erase(it, m_tradeRoutes.end());
}

const std::vector<TradeRoute_t>& ResourceManager::GetTradeRoutes() const
{
    return m_tradeRoutes;
}

int ResourceManager::CalculateNutrients_() const
{
    if (!m_tileLookup || !m_pWorkerAssignments)
    {
        return 0;
    }
    const TileResources_t worked = m_pWorkerAssignments->ComputeWorkedResources(
        m_pPopulation->GetContainer(), m_tileLookup);
    // TODO: Add nutrient bonuses from buildings
    return worked.nutrients;
}

int ResourceManager::CalculateEnergyProduction_() const
{
    int totalEnergy = 0;

    if (m_tileLookup && m_pWorkerAssignments)
    {
        const TileResources_t worked = m_pWorkerAssignments->ComputeWorkedResources(
            m_pPopulation->GetContainer(), m_tileLookup);
        totalEnergy += worked.energy;
    }

    // Energy from specialists
    // TODO: When specialist types are implemented, calculate actual energy contribution
    int specialistCount = m_pPopulation->GetSpecialistCount();
    (void)specialistCount;

    // Energy from trade routes
    for (const auto& route : m_tradeRoutes)
    {
        totalEnergy += route.energyBonus;
    }

    // TODO: Add energy production from buildings
    for (const auto& building : m_buildings)
    {
        (void)building;
    }

    return totalEnergy;
}

int ResourceManager::CalculateMinerals_() const
{
    if (!m_tileLookup || !m_pWorkerAssignments)
    {
        return 0;
    }
    const TileResources_t worked = m_pWorkerAssignments->ComputeWorkedResources(
        m_pPopulation->GetContainer(), m_tileLookup);
    // TODO: Add mineral bonuses from buildings
    // TODO: Remove minerals from unit upkeep
    return worked.minerals;
}

int ResourceManager::GetNutrientProduction() const
{
    return CalculateNutrients_();
}

int ResourceManager::GetEnergyProduction() const
{
    return CalculateEnergyProduction_();
}

int ResourceManager::GetMineralProduction() const
{
    return CalculateMinerals_();
}

int ResourceManager::GetNutrientStockpile() const
{
    return m_nutrientStockpile;
}

int ResourceManager::GetMineralStockpile() const
{
    return m_mineralStockpile;
}

void ResourceManager::AccumulateStockpiles()
{
    m_nutrientStockpile += CalculateNutrients_();
    m_mineralStockpile  += CalculateMinerals_();
    // Energy is not accumulated - it flows directly to faction-level allocation
}

int ResourceManager::ConsumeNutrients(int amount)
{
    int consumed = std::min(amount, m_nutrientStockpile);
    m_nutrientStockpile -= consumed;
    return consumed;
}

int ResourceManager::ConsumeMinerals(int amount)
{
    int consumed = std::min(amount, m_mineralStockpile);
    m_mineralStockpile -= consumed;
    return consumed;
}

} // namespace ac

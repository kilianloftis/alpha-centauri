#include "game/faction/Base.h"
#include "game/faction/BasePopulation.h"

namespace ac
{

Base::Base()
    : m_pPopulation(std::make_unique<BasePopulation>())
    , m_workedTiles{0, 0, 0}
{
}

Base::~Base()
{
}

BasePopulation* Base::GetPopulation()
{
    return m_pPopulation.get();
}

const BasePopulation* Base::GetPopulation() const
{
    return m_pPopulation.get();
}

void Base::AddBuilding(const std::string& buildingId)
{
    m_buildings.push_back(buildingId);
}

void Base::RemoveBuilding(const std::string& buildingId)
{
    auto it = std::find(m_buildings.begin(), m_buildings.end(), buildingId);
    if (it != m_buildings.end())
    {
        m_buildings.erase(it);
    }
}

const std::vector<std::string>& Base::GetBuildings() const
{
    return m_buildings;
}

void Base::SetWorkedTiles(const TileResources_t& resources)
{
    m_workedTiles = resources;
}

TileResources_t Base::GetWorkedTiles() const
{
    return m_workedTiles;
}

void Base::AddTradeRoute(const TradeRoute_t& tradeRoute)
{
    m_tradeRoutes.push_back(tradeRoute);
}

void Base::RemoveTradeRoute(int targetFactionId)
{
    auto it = std::remove_if(m_tradeRoutes.begin(), m_tradeRoutes.end(),
        [targetFactionId](const TradeRoute_t& route) {
            return route.targetFactionId == targetFactionId;
        });
    m_tradeRoutes.erase(it, m_tradeRoutes.end());
}

const std::vector<TradeRoute_t>& Base::GetTradeRoutes() const
{
    return m_tradeRoutes;
}

int Base::CalculateNutrients_() const
{
    // Tiles are either worked or not - provide full resources or none
    // TODO: Determine which tiles are worked based on worker assignments
    return m_workedTiles.nutrients;
}

int Base::CalculateEnergyProduction_() const
{
    int totalEnergy = 0;
    
    // Energy from worked tiles
    // TODO: Determine which tiles are worked based on worker assignments
    totalEnergy += m_workedTiles.energy;
    
    // Energy from econ workers
    // TODO: Determine energy production per econ worker
    int econCount = m_pPopulation->GetWorkerCount(WorkerRole::Econ);
    
    // Energy from trade routes
    for (const auto& route : m_tradeRoutes)
    {
        totalEnergy += route.energyBonus;
    }
    
    // Energy from buildings
    for (const auto& building : m_buildings)
    {
        // TODO: Add energy production from buildings
    }
    
    return totalEnergy;
}

int Base::CalculateMinerals_() const
{
    int totalMinerals = 0;
    
    // Minerals from worked tiles
    // TODO: Determine which tiles are worked based on worker assignments
    totalMinerals = m_workedTiles.minerals;
    
    // Minerals from buildings
    for (const auto& building : m_buildings)
    {
        // TODO: Add minerals from buildings
    }
    
    // TODO: Remove minerals from unit upkeep
    
    return totalMinerals;
}

void Base::SetName(const std::string& name)
{
    m_name = name;
}

const std::string& Base::GetName() const
{
    return m_name;
}

int Base::GetEnergyProduction() const
{
    return CalculateEnergyProduction_();
}

void Base::ApplyProduction_()
{
    // TODO: Apply production to faction economy
}

void Base::ApplyNutrition_()
{
    // TODO: Apply nutrition to population growth
}

void Base::ApplyPsych_()
{
    // TODO: Apply psych to faction
}

} // namespace ac

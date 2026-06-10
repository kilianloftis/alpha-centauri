#include "game/faction/base/BuildingManager.h"
#include "game/buildings/Building.h"
#include "game/buildings/BuildingFactory.h"
#include <algorithm>

namespace ac
{

BuildingManager::BuildingManager(const BuildingFactory* pFactory)
    : m_pFactory(pFactory)
{
}

BuildingManager::~BuildingManager()
{
}

void BuildingManager::AddBuilding(const std::string& buildingId)
{
    m_buildings.push_back(m_pFactory->CreateBuilding(buildingId));
}

void BuildingManager::DestroyBuilding(const std::string& buildingId)
{
    auto it = std::find_if(m_buildings.begin(), m_buildings.end(),
        [&buildingId](const std::unique_ptr<Building>& pBuilding)
        {
            return buildingId == pBuilding->GetBuildingId();
        });

    if (it != m_buildings.end())
    {
        m_buildings.erase(it);
    }
}

const std::vector<std::unique_ptr<Building>>& BuildingManager::GetBuildings() const
{
    return m_buildings;
}

int BuildingManager::GetTotalNutrientsBonus() const
{
    int total = 0;
    for (const auto& pBuilding : m_buildings)
    {
        total += pBuilding->GetNutrientsBonus();
    }
    return total;
}

int BuildingManager::GetTotalImprovementNutrientsBonus(const std::string& improvementName) const
{
    int total = 0;
    for (const auto& pBuilding : m_buildings)
    {
        total += pBuilding->GetImprovementNutrientsBonus(improvementName);
    }
    return total;
}

} // namespace ac

#include "game/faction/base/buildings/BuildingManager.h"
#include "game/buildings/Building.h"
#include "game/buildings/BuildingRegistry.h"
#include <algorithm>

namespace ac
{

BuildingManager::BuildingManager(const BuildingRegistry* pRegistry)
    : m_pRegistry(pRegistry)
{
}

BuildingManager::~BuildingManager()
{
}

void BuildingManager::AddBuilding(const std::string& buildingId)
{
    m_buildings.push_back(m_pRegistry->Create(buildingId));
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

std::vector<const Building*> BuildingManager::GetBuildingsAvailableForConstruction(const std::vector<const Building*>& discoveredBuildings) const
{
    std::vector<const Building*> available;
    for (const Building* pBuilding : discoveredBuildings)
    {
        if (pBuilding && (pBuilding->GetAllowMultiple() || !DoesBuildingExist_(pBuilding->GetBuildingId())))
        {
            available.push_back(pBuilding);
        }
    }
    return available;
}

bool BuildingManager::DoesBuildingExist_(const std::string& buildingId) const
{
    return std::find_if(m_buildings.begin(), m_buildings.end(),
        [&buildingId](const std::unique_ptr<Building>& pBuilding)
        {
            return buildingId == pBuilding->GetBuildingId();
        }) != m_buildings.end();
}

} // namespace ac

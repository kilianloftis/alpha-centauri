#include "game/buildings/Building.h"
#include "game/buildings/BuildingConfigParser.h"

namespace ac
{

Building::Building(const BuildingConfig_t& rConfig)
    : m_pConfig(&rConfig)
{
}

Building::~Building()
{
}

const char* Building::GetBuildingId() const
{
    return m_pConfig->id.c_str();
}

const std::string& Building::GetName() const
{
    return m_pConfig->name;
}

int Building::GetMineralCost() const
{
    return m_pConfig->mineralCost;
}

bool Building::GetAllowMultiple() const
{
    return m_pConfig->allowMultiple;
}

int Building::GetNutrientsBonus() const
{
    return m_pConfig->nutrientsBonus;
}

int Building::GetImprovementNutrientsBonus(const std::string& improvementName) const
{
    auto it = m_pConfig->improvementBonuses.find(improvementName);
    if (it == m_pConfig->improvementBonuses.end())
    {
        return 0;
    }
    return it->second.nutrients;
}

} // namespace ac

#include "game/research/Tech.h"
#include "game/research/TechConfigParser.h"

namespace ac
{

Tech::Tech()
    : m_id()
    , m_name()
    , m_description()
    , m_prerequisites()
    , m_baseCost(100)
{
}

Tech::Tech(const TechConfig& rConfig)
    : m_id(rConfig.id)
    , m_name(rConfig.name)
    , m_description()
    , m_category(rConfig.category)
    , m_prerequisites(rConfig.prerequisites)
    , m_baseCost(rConfig.cost)
{
}

Tech::~Tech()
{
}

TechId Tech::GetId() const
{
    return m_id;
}

const std::string& Tech::GetName() const
{
    return m_name;
}

const std::string& Tech::GetDescription() const
{
    return m_description;
}

const std::string& Tech::GetCategory() const
{
    return m_category;
}

void Tech::AddPrerequisite(TechId techId)
{
    m_prerequisites.push_back(techId);
}

const std::vector<TechId>& Tech::GetPrerequisites() const
{
    return m_prerequisites;
}

bool Tech::HasPrerequisite(TechId techId) const
{
    for (TechId prereq : m_prerequisites)
    {
        if (prereq == techId)
        {
            return true;
        }
    }
    return false;
}

void Tech::SetBaseCost(int cost)
{
    m_baseCost = cost;
}

int Tech::GetBaseCost() const
{
    return m_baseCost;
}

} // namespace ac

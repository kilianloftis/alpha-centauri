#include "game/research/Tech.h"

namespace ac
{

Tech::Tech()
    : m_id(0)
    , m_name()
    , m_description()
    , m_prerequisites()
    , m_baseCost(100)
{
}

Tech::Tech(TechId id, std::string name, std::string description)
    : m_id(id)
    , m_name(std::move(name))
    , m_description(std::move(description))
    , m_prerequisites()
    , m_baseCost(100)
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

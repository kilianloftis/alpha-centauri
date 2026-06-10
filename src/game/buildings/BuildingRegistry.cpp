#include "game/buildings/BuildingRegistry.h"
#include "game/buildings/BuildingConfigParser.h"

namespace ac
{

BuildingRegistry::BuildingRegistry()
{
}

bool BuildingRegistry::Load(const std::string& configPath)
{
    BuildingConfigParser parser;
    auto configs = parser.ParseConfig(configPath);
    if (configs.empty())
    {
        return false;
    }

    m_configs = std::move(configs);
    m_indexById.clear();
    for (size_t i = 0; i < m_configs.size(); i++)
    {
        m_indexById[m_configs[i].id] = i;
    }
    return true;
}

const BuildingConfig* BuildingRegistry::Find(const std::string& id) const
{
    auto it = m_indexById.find(id);
    if (it == m_indexById.end())
    {
        return nullptr;
    }
    return &m_configs[it->second];
}

const std::vector<BuildingConfig>& BuildingRegistry::GetAll() const
{
    return m_configs;
}

} // namespace ac

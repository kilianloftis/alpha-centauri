#include "game/faction/base/population/pop-types/PopTypeRegistry.h"
#include "game/faction/base/population/pop-types/PopTypeConfigParser.h"

namespace ac
{

PopTypeRegistry::PopTypeRegistry()
{
}

bool PopTypeRegistry::Load(const std::string& configPath)
{
    PopTypeConfigParser parser;
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

const PopTypeConfig* PopTypeRegistry::Find(const std::string& id) const
{
    auto it = m_indexById.find(id);
    if (it == m_indexById.end())
    {
        return nullptr;
    }
    return &m_configs[it->second];
}

const std::vector<PopTypeConfig>& PopTypeRegistry::GetAll() const
{
    return m_configs;
}

} // namespace ac

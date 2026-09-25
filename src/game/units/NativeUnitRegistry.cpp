#include "game/units/NativeUnitRegistry.h"

#include <stdexcept>
#include <utility>

namespace ac
{

void NativeUnitRegistry::Load(const std::string& rConfigPath)
{
    NativeUnitConfigParser parser;
    std::vector<NativeUnitConfig_t> units = parser.ParseConfig(rConfigPath);

    std::vector<NativeUnitConfig_t> previousConfigs = std::move(m_configs);
    std::unordered_map<std::string, size_t> previousIndex = std::move(m_indexById);

    m_configs = std::move(units);
    m_indexById.clear();
    for (size_t i = 0; i < m_configs.size(); ++i)
    {
        m_indexById[m_configs[i].id] = i;
    }

    try
    {
        Validate_();
    }
    catch (...)
    {
        m_configs = std::move(previousConfigs);
        m_indexById = std::move(previousIndex);
        throw;
    }

    m_lifeforms = parser.Life();
}

} // namespace ac

#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

// Generic config registry: loads TConfig entries via TParser::ParseConfig(),
// stores them keyed by TConfig::id, and exposes Find() / GetAll() / Create().
// TParser must provide: std::vector<TConfig> ParseConfig(const std::string&)
// TConfig must have: std::string id
template<typename TConfig, typename TParser, typename TCreated = TConfig>
class Registry
{
public:
    Registry() = default;
    ~Registry() = default;

    bool Load(const std::string& rConfigPath)
    {
        TParser parser;
        auto configs = parser.ParseConfig(rConfigPath);
        if (configs.empty())
        {
            throw std::runtime_error("No configs found in " + rConfigPath);
        }

        m_configs = std::move(configs);
        m_indexById.clear();
        for (size_t i = 0; i < m_configs.size(); i++)
        {
            m_indexById[m_configs[i].id] = i;
        }
        return true;
    }

    const TConfig* Find(const std::string& rId) const
    {
        auto it = m_indexById.find(rId);
        if (it == m_indexById.end())
        {
            return nullptr;
        }
        return &m_configs[it->second];
    }

    const std::vector<TConfig>& GetAll() const
    {
        return m_configs;
    }

    // Create a TCreated instance for the given id.
    // Throws std::runtime_error if id is not found.
    std::unique_ptr<TCreated> Create(const std::string& rId) const
    {
        const TConfig* pConfig = Find(rId);
        if (!pConfig)
        {
            throw std::runtime_error("Unknown id '" + rId + "'");
        }
        return std::make_unique<TCreated>(*pConfig);
    }

protected:
    std::vector<TConfig> m_configs;
    std::unordered_map<std::string, size_t> m_indexById;
};

} // namespace ac

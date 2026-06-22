#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace ac
{

// Generic config registry: loads TConfig entries via TParser::ParseConfig(),
// stores them keyed by TConfig::id, and exposes Find() / GetAll() / Create().
// TParser must provide: std::vector<TConfig> ParseConfig(const std::string&)
// TConfig must have: std::string id
// When TCreated differs from TConfig, GetAllCreated() returns a stable vector of
// TCreated prototypes (one per config), constructed via TCreated(const TConfig&).
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

        if constexpr (!std::is_same_v<TConfig, TCreated>)
        {
            m_prototypes.clear();
            for (const auto& config : m_configs)
            {
                m_prototypes.emplace_back(config);
            }
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

    // Returns stable TCreated prototypes, one per config entry.
    // Only available when TCreated differs from TConfig.
    // Pointers are valid for the lifetime of this registry.
    template<typename T = TCreated>
    std::enable_if_t<!std::is_same_v<TConfig, T>, const std::vector<T>&>
    GetAllCreated() const
    {
        return m_prototypes;
    }

protected:
    std::vector<TConfig> m_configs;
    std::unordered_map<std::string, size_t> m_indexById;
    std::vector<TCreated> m_prototypes;
};

} // namespace ac

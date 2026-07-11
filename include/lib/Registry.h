#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

// Generic config registry: loads TConfig entries via TParser::ParseConfig(),
// stores them keyed by TConfig::id, and exposes Find() / Get() / GetAll() / Create().
// Find() is the optional probe (nullptr on miss); Get()/Create() throw on unknown ids.
// TParser must provide: std::vector<TConfig> ParseConfig(const std::string&)
// TConfig must have: std::string id
// TInstance defaults to TConfig. When overridden, Create() constructs a
// TInstance from a TConfig via TInstance(const TConfig&).
template<typename TConfig, typename TParser, typename TInstance = TConfig>
class Registry
{
public:
    Registry() = default;
    // Virtual: subclasses override the protected virtual Validate_ (e.g. TechRegistry,
    // PopTypeRegistry), so the base is an inheritance point and must be safe to delete
    // through a base pointer.
    virtual ~Registry() = default;

    void Load(const std::string& rConfigPath)
    {
        TParser parser;
        auto configs = parser.ParseConfig(rConfigPath);

        m_indexById.clear();
        m_configs = std::move(configs);
        for (size_t i = 0; i < m_configs.size(); i++)
        {
            m_indexById[m_configs[i].id] = i;
        }

        Validate_();
    }

    // Optional lookup: nullptr when the id is absent. Prefer Get() when the id must exist.
    const TConfig* Find(const std::string& rId) const
    {
        auto it = m_indexById.find(rId);
        if (it == m_indexById.end())
        {
            return nullptr;
        }
        return &m_configs[it->second];
    }

    // Required lookup: throws std::runtime_error if the id is not found.
    const TConfig& Get(const std::string& rId) const
    {
        const TConfig* pConfig = Find(rId);
        if (!pConfig)
        {
            throw std::runtime_error("Unknown id '" + rId + "'");
        }
        return *pConfig;
    }

    const std::vector<TConfig>& GetAll() const
    {
        return m_configs;
    }

    // Create an owned TInstance for the given id.
    // Throws std::runtime_error if id is not found.
    std::unique_ptr<TInstance> Create(const std::string& rId) const
    {
        return std::make_unique<TInstance>(Get(rId));
    }

protected:
    virtual void Validate_()
    {
        ValidateNoDuplicates_();
    }

    void ValidateNoDuplicates_() const
    {
        for (const TConfig& rConfig : m_configs)
        {
            int count = 0;
            for (const TConfig& rOther : m_configs)
            {
                if (rOther.id == rConfig.id)
                {
                    ++count;
                }
                if (count > 1)
                {
                    throw std::runtime_error("Duplicate id '" + rConfig.id + "' in registry");
                }
            }
        }
    }

    std::vector<TConfig> m_configs;
    std::unordered_map<std::string, size_t> m_indexById;
};

} // namespace ac

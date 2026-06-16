#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialPolicyConfigParser.h"

namespace ac
{

SocialPolicyRegistry::SocialPolicyRegistry()
{
}

bool SocialPolicyRegistry::Load(const std::string& configPath)
{
    SocialPolicyConfigParser parser;
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

const SocialPolicyConfig* SocialPolicyRegistry::Find(const std::string& id) const
{
    auto it = m_indexById.find(id);
    if (it == m_indexById.end())
    {
        return nullptr;
    }
    return &m_configs[it->second];
}

const std::vector<SocialPolicyConfig>& SocialPolicyRegistry::GetAll() const
{
    return m_configs;
}

std::vector<const SocialPolicyConfig*> SocialPolicyRegistry::GetByCategory(SocialCategory category) const
{
    std::vector<const SocialPolicyConfig*> result;
    for (const auto& config : m_configs)
    {
        if (config.category == category)
        {
            result.push_back(&config);
        }
    }
    return result;
}

} // namespace ac

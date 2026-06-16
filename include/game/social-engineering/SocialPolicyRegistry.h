#pragma once

#include "game/social-engineering/SocialPolicyConfig.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

// Holds all SocialPolicyConfig entries loaded from config, keyed by id.
// Loaded once at startup via Load(); queried at runtime via Find().
class SocialPolicyRegistry
{
public:
    SocialPolicyRegistry();
    ~SocialPolicyRegistry() = default;

    // Load all social policies from a config file. Returns false on failure.
    bool Load(const std::string& configPath);

    // Find a policy by id. Returns nullptr if not found.
    const SocialPolicyConfig* Find(const std::string& id) const;

    // All loaded policies in definition order.
    const std::vector<SocialPolicyConfig>& GetAll() const;

    // All policies belonging to a specific category.
    std::vector<const SocialPolicyConfig*> GetByCategory(SocialCategory category) const;

private:
    std::vector<SocialPolicyConfig> m_configs;
    std::unordered_map<std::string, size_t> m_indexById;
};

} // namespace ac

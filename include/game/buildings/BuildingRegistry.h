#pragma once

#include "game/buildings/BuildingConfigParser.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

// Holds all BuildingConfig entries loaded from config, keyed by id.
// Loaded once at startup via Load(); queried at runtime via Find().
class BuildingRegistry
{
public:
    BuildingRegistry();
    ~BuildingRegistry() = default;

    // Load all buildings from a config file. Returns false on failure.
    bool Load(const std::string& configPath);

    // Find a building by id. Returns nullptr if not found.
    const BuildingConfig* Find(const std::string& id) const;

    // All loaded configs in definition order.
    const std::vector<BuildingConfig>& GetAll() const;

private:
    std::vector<BuildingConfig> m_configs;
    std::unordered_map<std::string, size_t> m_indexById;
};

} // namespace ac

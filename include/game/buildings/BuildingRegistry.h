#pragma once

#include "game/buildings/BuildingConfigParser.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

class Building;

// Holds all BuildingConfig_t entries loaded from config, keyed by id.
// Loaded once at startup via Load(); queried at runtime via Find().
class BuildingRegistry
{
public:
    BuildingRegistry();
    ~BuildingRegistry() = default;

    // Load all buildings from a config file. Returns false on failure.
    bool Load(const std::string& configPath);

    // Find a building by id. Returns nullptr if not found.
    const BuildingConfig_t* Find(const std::string& id) const;

    // All loaded configs in definition order.
    const std::vector<BuildingConfig_t>& GetAll() const;

    // Create a Building instance for the given id.
    // Throws std::runtime_error if id is not found.
    std::unique_ptr<Building> CreateBuilding(const std::string& id) const;

private:
    std::vector<BuildingConfig_t> m_configs;
    std::unordered_map<std::string, size_t> m_indexById;
};

} // namespace ac

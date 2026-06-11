#pragma once

#include "game/faction/base/population/pop-types/PopTypeConfigParser.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

class Pop;

// Holds all PopTypeConfig entries loaded from config, keyed by id.
// Loaded once at startup via Load(); queried at runtime via Find().
class PopTypeRegistry
{
public:
    PopTypeRegistry();
    ~PopTypeRegistry() = default;

    // Load all pop types from a config file. Returns false on failure.
    bool Load(const std::string& configPath);

    // Find a pop type by id. Returns nullptr if not found.
    const PopTypeConfig* Find(const std::string& id) const;

    // All loaded configs in definition order.
    const std::vector<PopTypeConfig>& GetAll() const;

    // Create a Pop instance for the given type id.
    // Throws std::runtime_error if typeId is not found.
    std::unique_ptr<Pop> CreatePop(const std::string& typeId) const;

private:
    std::vector<PopTypeConfig> m_configs;
    std::unordered_map<std::string, size_t> m_indexById;
};

} // namespace ac

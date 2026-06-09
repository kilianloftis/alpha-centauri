#pragma once

#include "game/map/TileBonusConfigParser.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

// Holds all TileBonusConfig entries loaded from config, keyed by id.
// Loaded once at startup via Load(); queried at runtime via Find().
class TileBonusRegistry
{
public:
    TileBonusRegistry();
    ~TileBonusRegistry() = default;

    // Load all tile bonuses from a config file. Returns false on failure.
    bool Load(const std::string& configPath);

    // Find a tile bonus by id. Returns nullptr if not found.
    const TileBonusConfig* Find(const std::string& id) const;

    // All loaded configs in definition order.
    const std::vector<TileBonusConfig>& GetAll() const;

private:
    std::vector<TileBonusConfig> m_configs;
    std::unordered_map<std::string, size_t> m_indexById;
};

} // namespace ac

#pragma once

#include "game/map/ElevationRulesConfig.h"
#include "game/map/WorldGenPresetConfig.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

class WorldGenPresetConfigParser
{
public:
    WorldGenPresetConfigParser() = default;
    ~WorldGenPresetConfigParser() = default;

    std::vector<WorldGenPresetConfig_t> ParseConfig(const std::string& configPath);

    // Preset remap range must sit inside the planet domain, with water below ocean level.
    static void ValidateAgainstMapRules(const WorldGenPresetConfig_t& rPreset,
                                        const ElevationRulesConfig_t& rMapRules);

private:
    WorldGenPresetConfig_t ParsePresetConfig_(const nlohmann::json& presetJson);
    WorldGenPreset_t ParseType_(const std::string& typeStr) const;
};

} // namespace ac

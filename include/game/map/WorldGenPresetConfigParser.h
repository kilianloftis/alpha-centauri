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

    // Preset min/max must bracket ocean level and cover the shelf and spread-altitude limit.
    static void ValidateAgainstMapRules(const WorldGenPresetConfig_t& rPreset,
                                        const ElevationRulesConfig_t& rMapRules);

    // Copies the preset's elevation range onto rRules after the same check.
    static void ApplyElevationRange(ElevationRulesConfig_t& rRules,
                                    const WorldGenPresetConfig_t& rPreset);

private:
    WorldGenPresetConfig_t ParsePresetConfig_(const nlohmann::json& presetJson);
    WorldGenPreset_t ParseType_(const std::string& typeStr) const;
};

} // namespace ac

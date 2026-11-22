#pragma once

#include "game/map/MapRulesConfig.h"
#include "game/worldgen/WorldGenPresetConfig.h"
#include <string>
#include <vector>

namespace ac
{

class ImprovementRegistry;

class WorldGenPresetConfigParser
{
public:
    static std::vector<WorldGenPresetConfig_t> ParseConfig(const std::string& configPath);

    // Preset min/max must bracket ocean level and cover the shelf.
    static void ValidateAgainstMapRules(const WorldGenPresetConfig_t& rPreset,
                                        const MapRulesConfig_t& rMapRules);

    // Preset max must cover every spreading improvement's altitude_limit_meters.
    static void ValidateAgainstSpreadLimits(const WorldGenPresetConfig_t& rPreset,
                                            const ImprovementRegistry& rOccupants);

    // Copies the preset's elevation range onto rRules after the same check.
    static void ApplyElevationRange(MapRulesConfig_t& rRules,
                                    const WorldGenPresetConfig_t& rPreset);
};

} // namespace ac

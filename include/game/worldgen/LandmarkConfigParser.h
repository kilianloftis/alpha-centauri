#pragma once

#include "game/worldgen/LandmarkConfig.h"
#include "game/worldgen/WorldGenPresetConfig.h"
#include <string>
#include <vector>

namespace ac
{

class LandmarkConfigParser
{
public:
    // Loads config/worldGen/landmarks.json, checking improvement ids when rKnownImprovementIds is
    // non-empty. Sculpt elevations are checked against the preset later.
    std::vector<LandmarkConfig_t> ParseConfig(
        const std::string& configPath,
        const std::vector<std::string>& rKnownImprovementIds = {});

    // A sculptor's peak and base must lie inside the preset elevation range.
    static void ValidateSculptAgainstPreset(const LandmarkConfig_t& rLandmark,
                                            const WorldGenPresetConfig_t& rPreset);
};

} // namespace ac

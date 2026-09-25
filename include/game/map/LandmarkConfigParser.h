#pragma once

#include "game/map/LandmarkConfig.h"
#include "game/map/WorldGenPresetConfig.h"
#include <string>
#include <vector>

namespace ac
{

class LandmarkConfigParser
{
public:
    // Load config/worldGen/landmarks.json (top-level array). Throws on parse errors.
    // Validates that every improvement_id exists when rKnownImprovementIds is non-empty.
    // Sculpt elevations are checked against the chosen preset, not here.
    std::vector<LandmarkConfig_t> ParseConfig(
        const std::string& configPath,
        const std::vector<std::string>& rKnownImprovementIds = {});

    // A sculptor's peak and base must lie inside the preset elevation range.
    static void ValidateSculptAgainstPreset(const LandmarkConfig_t& rLandmark,
                                            const WorldGenPresetConfig_t& rPreset);
};

} // namespace ac

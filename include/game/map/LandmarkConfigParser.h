#pragma once

#include "game/map/LandmarkConfig.h"
#include <string>
#include <vector>

namespace ac
{

class LandmarkConfigParser
{
public:
    // Load config/worldGen/landmarks.json (top-level array). Throws on parse errors.
    // Validates that every improvement_id exists when rKnownImprovementIds is non-empty.
    std::vector<LandmarkConfig_t> ParseConfig(
        const std::string& configPath,
        const std::vector<std::string>& rKnownImprovementIds = {});
};

} // namespace ac

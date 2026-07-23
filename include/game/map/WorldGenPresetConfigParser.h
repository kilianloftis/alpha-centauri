#pragma once

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

private:
    WorldGenPresetConfig_t ParsePresetConfig_(const nlohmann::json& presetJson);
    WorldGenPreset_t ParseType_(const std::string& typeStr) const;
};

} // namespace ac

#pragma once

#include <string>

namespace ac
{

// Player/session knobs for world generation. Landmass recipe knobs live on
// WorldGenPresetConfig_t (loaded from config/world_gen_presets.json).
struct MapGenerationConfig_t
{
    int width = 200;
    int height = 150;
    unsigned int seed = 0;              // 0 = random
    float oceanCoverage = 0.6f;         // target water fraction [0,1]
    std::string presetId = "continents";
};

} // namespace ac

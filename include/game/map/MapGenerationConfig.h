#pragma once

#include <string>

namespace ac
{

// Hardcoded new-game menu options for rockiness frequency.
// Higher erosion → flatter terrain (classic SMAC). Weights live in decoration.json.
enum class ErosiveForces_t
{
    Low,
    Average,
    High,
};

std::string ToString(ErosiveForces_t erosiveForces);
ErosiveForces_t ParseErosiveForces(const std::string& value);

// Player/session knobs for world generation. Landmass recipe knobs live on
// WorldGenPresetConfig_t (loaded from config/worldGen/presets.json).
struct MapGenerationConfig_t
{
    int width = 200;
    int height = 150;
    unsigned int seed = 0;              // 0 = random
    float oceanCoverage = 0.6f;         // target water fraction [0,1]
    ErosiveForces_t erosiveForces = ErosiveForces_t::Average;
    std::string presetId = "islands";

    bool operator==(const MapGenerationConfig_t&) const = default;
};

} // namespace ac

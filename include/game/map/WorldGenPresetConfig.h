#pragma once

#include <string>

namespace ac
{

// Closed set of landmass algorithms. Tuning for each named preset lives in JSON;
// this enum selects mask/shaping branches in WorldGenerator.
enum class WorldGenPreset_t
{
    Pangea,
    Continents,
    Islands,
    Archipelago,
    Ring,
    Lakes,
};

struct WorldGenPresetConfig_t
{
    std::string id;
    std::string name;
    WorldGenPreset_t type = WorldGenPreset_t::Continents;

    // FBM / FastNoiseLite
    float frequency = 0.02f;
    int octaves = 5;
    float lacunarity = 2.0f;
    float gain = 0.5f;

    // Landmass shape (masks applied after noise; not water fraction)
    float continentScale = 1.0f;
    float centerBias = 0.0f;
    float edgeFalloff = 0.0f;

    // Elevation remap of final noise to meters
    int minElevation = -4000;
    int maxElevation = 4000;
};

} // namespace ac

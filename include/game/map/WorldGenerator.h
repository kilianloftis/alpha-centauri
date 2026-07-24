#pragma once

#include "game/map/MapGenerationConfig.h"
#include "game/map/WorldGenDecorationConfig.h"
#include "game/map/WorldGenPresetConfig.h"
#include "game/map/WorldMap.h"
#include <memory>
#include <random>

namespace ac
{

class WorldGenerator
{
public:
    WorldGenerator();
    ~WorldGenerator();

    // Generate a new world map from session knobs + a resolved landmass preset
    // and post-elevation decoration (moisture, …).
    std::unique_ptr<WorldMap> Generate(const MapGenerationConfig_t& rConfig,
                                       const WorldGenPresetConfig_t& rPreset,
                                       const WorldGenDecorationConfig_t& rDecoration);

private:
    std::mt19937 m_rng;

    void GenerateElevation_(WorldMap& rWorld,
                            const MapGenerationConfig_t& rConfig,
                            const WorldGenPresetConfig_t& rPreset);
    void GenerateMoisture_(WorldMap& rWorld, const MoistureDecorationConfig_t& rMoisture);
    void GenerateRockiness_(WorldMap& rWorld);

    float ApplyLandmassMask_(float noiseValue,
                             float nx,
                             float ny,
                             const WorldGenPresetConfig_t& rPreset) const;

    int RandomInt_(int min, int max);
    float RandomFloat_();
};

} // namespace ac

#pragma once

#include "game/map/ElevationRulesConfig.h"
#include "game/worldgen/LandmarkConfig.h"
#include "game/worldgen/MapGenerationConfig.h"
#include "game/worldgen/WorldGenDecorationConfig.h"
#include "game/worldgen/WorldGenPresetConfig.h"
#include "game/map/WorldMap.h"
#include <memory>
#include <random>
#include <vector>

namespace ac
{

class ImprovementRegistry;
class FbmNoise;

class WorldGenerator
{
public:
    WorldGenerator();
    ~WorldGenerator();

    // Generates a world from the session knobs and a landmass preset. seed is the session seed the
    // composition root resolved, so the reported seed reproduces the map.
    std::unique_ptr<WorldMap> Generate(const MapGenerationConfig_t& rConfig,
                                       const WorldGenPresetConfig_t& rPreset,
                                       const WorldGenDecorationConfig_t& rDecoration,
                                       const std::vector<LandmarkConfig_t>& rLandmarks,
                                       const ImprovementRegistry& rOccupants,
                                       const ElevationRulesConfig_t& rMapRules,
                                       unsigned int seed);

private:
    std::mt19937 m_rng;

    void GenerateElevation_(WorldMap& rWorld,
                            const MapGenerationConfig_t& rConfig,
                            const WorldGenPresetConfig_t& rPreset);
    void GenerateRockiness_(WorldMap& rWorld,
                            ErosiveForces_t erosiveForces,
                            const RockinessDecorationConfig_t& rRockiness);
    void GenerateAquifers_(WorldMap& rWorld, const AquiferDecorationConfig_t& rAquifers,
                           const ImprovementRegistry& rOccupants);

    // Masked noise per tile, indexed by TileIndex.
    std::vector<float> SampleElevationField_(const WorldMap& rWorld, const FbmNoise& rNoise,
                                             const WorldGenPresetConfig_t& rPreset) const;
    float ApplyLandmassMask_(float noiseValue,
                             float nx,
                             float ny,
                             const WorldGenPresetConfig_t& rPreset) const;

    float RandomFloat_();
};

} // namespace ac

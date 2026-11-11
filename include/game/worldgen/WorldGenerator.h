#pragma once

#include "game/map/MapRulesConfig.h"
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
class SupplyPodIndex;
struct SupplyPodConfig_t;

class WorldGenerator
{
public:
    WorldGenerator();
    ~WorldGenerator();

    // Generates a world from the session knobs and a landmass preset. seed is the session seed the
    // composition root resolved, so the reported seed reproduces the map. fungusBandWidening comes
    // from the session's native life level. Supply pods land after landmarks and before tile
    // bonuses (stamped into rSupplyPods); bNoUnityScattering skips global / wreckage pods.
    std::unique_ptr<WorldMap> Generate(const MapGenerationConfig_t& rConfig,
                                       const WorldGenPresetConfig_t& rPreset,
                                       const WorldGenDecorationConfig_t& rDecoration,
                                       int fungusBandWidening,
                                       const std::vector<LandmarkConfig_t>& rLandmarks,
                                       const ImprovementRegistry& rOccupants,
                                       const MapRulesConfig_t& rMapRules,
                                       const SupplyPodConfig_t& rSupplyPods,
                                       SupplyPodIndex& rSupplyPodIndex,
                                       bool bNoUnityScattering,
                                       unsigned int seed);

private:
    std::mt19937 m_rng;

    void GenerateElevation_(WorldMap& rWorld,
                            const MapGenerationConfig_t& rConfig,
                            const WorldGenPresetConfig_t& rPreset);
    void GenerateAquifers_(WorldMap& rWorld, const AquiferDecorationConfig_t& rAquifers,
                           const ImprovementRegistry& rOccupants);

    // Masked noise per tile, indexed by TileIndex.
    std::vector<float> SampleElevationField_(const WorldMap& rWorld, const FbmNoise& rNoise,
                                             const WorldGenPresetConfig_t& rPreset) const;
    float ApplyLandmassMask_(float noiseValue,
                             float nx,
                             float ny,
                             const WorldGenPresetConfig_t& rPreset) const;
};

} // namespace ac

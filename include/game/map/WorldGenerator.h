#pragma once

#include "game/map/LandmarkConfig.h"
#include "game/map/MapGenerationConfig.h"
#include "game/map/WorldGenDecorationConfig.h"
#include "game/map/WorldGenPresetConfig.h"
#include "game/map/WorldMap.h"
#include <memory>
#include <random>
#include <vector>

namespace ac
{

class ImprovementRegistry;

class WorldGenerator
{
public:
    WorldGenerator();
    ~WorldGenerator();

    // Generate a new world map from session knobs + a resolved landmass preset
    // and post-elevation decoration (moisture, aquifers, landmarks, …).
    //
    // seed is supplied by the caller rather than re-derived from rConfig.seed here: the
    // composition root resolves one session seed (and reports it), so a map generated from a
    // config seed of 0 must come from that resolved value. Deriving a time-based seed here
    // instead meant the reported session seed did not reproduce the map.
    std::unique_ptr<WorldMap> Generate(const MapGenerationConfig_t& rConfig,
                                       const WorldGenPresetConfig_t& rPreset,
                                       const WorldGenDecorationConfig_t& rDecoration,
                                       const std::vector<LandmarkConfig_t>& rLandmarks,
                                       const ImprovementRegistry& rImprovements,
                                       unsigned int seed);

private:
    std::mt19937 m_rng;

    void GenerateElevation_(WorldMap& rWorld,
                            const MapGenerationConfig_t& rConfig,
                            const WorldGenPresetConfig_t& rPreset);
    void GenerateMoisture_(WorldMap& rWorld, const MoistureDecorationConfig_t& rMoisture);
    void GenerateRockiness_(WorldMap& rWorld,
                            ErosiveForces_t erosiveForces,
                            const RockinessDecorationConfig_t& rRockiness);
    void GenerateAquifers_(WorldMap& rWorld, const AquiferDecorationConfig_t& rAquifers);
    void GenerateFungus_(WorldMap& rWorld, const FungusDecorationConfig_t& rFungus);
    void GenerateLandmarks_(WorldMap& rWorld,
                            const std::vector<LandmarkConfig_t>& rLandmarks,
                            const ImprovementRegistry& rImprovements);
    void GenerateTileBonuses_(WorldMap& rWorld,
                              const TileBonusDecorationConfig_t& rBonuses,
                              const ImprovementRegistry& rImprovements);

    float ApplyLandmassMask_(float noiseValue,
                             float nx,
                             float ny,
                             const WorldGenPresetConfig_t& rPreset) const;

    int RandomInt_(int min, int max);
    float RandomFloat_();
};

} // namespace ac

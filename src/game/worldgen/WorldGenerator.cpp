#include "game/worldgen/WorldGenerator.h"
#include "game/worldgen/WorldGenPresetConfigParser.h"
#include "game/worldgen/FbmNoise.h"
#include "game/worldgen/FungusGeneration.h"
#include "game/worldgen/TileBonusGeneration.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/worldgen/LandmarkGeneration.h"
#include "game/map/OccupantCoexistence.h"
#include "game/worldgen/MoistureGeneration.h"
#include "game/map/RiverGeneration.h"
#include "game/map/MapUtils.h"
#include "game/worldgen/RockinessGeneration.h"

#include <algorithm>
#include <string>
#include <chrono>
#include <cmath>
#include <vector>

namespace ac
{

namespace
{

float Remap_(float value, float inMin, float inMax, float outMin, float outMax)
{
    if (inMax <= inMin)
    {
        return outMin;
    }
    const float t = (value - inMin) / (inMax - inMin);
    return outMin + t * (outMax - outMin);
}

struct ElevationScale_t
{
    float minValue;
    float threshold;
    float maxValue;
};

// The field value below which the requested share of tiles is water.
float WaterThreshold_(const std::vector<float>& rField, float requestedCoverage)
{
    const float oceanCoverage = std::clamp(requestedCoverage, 0.0f, 1.0f);
    const size_t tileCount = rField.size();
    size_t waterCount = static_cast<size_t>(oceanCoverage * static_cast<float>(tileCount));
    if (waterCount >= tileCount)
    {
        waterCount = tileCount - 1;
    }
    std::vector<float> sorted = rField;
    std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(waterCount),
                     sorted.end());
    return sorted[waterCount];
}

int ElevationFor_(float value, const ElevationScale_t& rScale,
                  const WorldGenPresetConfig_t& rPreset)
{
    if (value < rScale.threshold)
    {
        const float belowWaterline = Remap_(value, rScale.minValue, rScale.threshold, 1.0f, 0.0f);
        const float depth = std::pow(belowWaterline, rPreset.oceanDepthExponent);
        const float deepest = static_cast<float>(rPreset.minElevation + 1);
        return static_cast<int>(std::lround(-1.0f + depth * deepest));
    }
    const float highest = static_cast<float>(rPreset.maxElevation);
    const float raised = Remap_(value, rScale.threshold, rScale.maxValue, 0.0f, highest);
    return static_cast<int>(std::lround(raised));
}

// Nothing on a new world shifts moisture, so each tile's moisture is its rainfall.
void ApplyBaseMoisture_(WorldMap& rWorld)
{
    for (const std::unique_ptr<Tile>& pTile : rWorld.GetTiles())
    {
        pTile->SetMoisture(pTile->GetBaseMoisture());
    }
}

} // namespace

WorldGenerator::WorldGenerator() = default;

WorldGenerator::~WorldGenerator() = default;

std::unique_ptr<WorldMap> WorldGenerator::Generate(const MapGenerationConfig_t& rConfig,
                                                   const WorldGenPresetConfig_t& rPreset,
                                                   const WorldGenDecorationConfig_t& rDecoration,
                                                   const std::vector<LandmarkConfig_t>& rLandmarks,
                                                   const ImprovementRegistry& rOccupants,
                                                   const MapRulesConfig_t& rMapRules,
                                                   unsigned int seed)
{
    m_rng.seed(seed);

    MapRulesConfig_t rules = rMapRules;
    WorldGenPresetConfigParser::ApplyElevationRange(rules, rPreset);
    auto pWorld = std::make_unique<WorldMap>(rConfig.width, rConfig.height, rules,
                                             rConfig.rainfall);

    // Bound before any stage runs: an unbound tile answers coexistence questions differently from
    // the same tile in play.
    for (const auto& pTile : pWorld->GetTiles())
    {
        pTile->BindOccupants(rOccupants);
    }

    // Landmarks are the last stage to change elevation or stamp terminates_river, so rivers follow
    // them; rain follows rivers (SMAC world_climate order); bonuses go last for their excludes.
    GenerateElevation_(*pWorld, rConfig, rPreset);
    GenerateRockiness_(*pWorld, rConfig.erosiveForces, rDecoration.rockiness);
    const ImprovementConfig_t& rFungus = rOccupants.Get(std::string(ImprovementIds::k_Fungus));
    PlaceFungus(*pWorld, rDecoration.fungus, rFungus, m_rng);
    PlaceLandmarks(*pWorld, rLandmarks, rOccupants, m_rng);
    GenerateAquifers_(*pWorld, rDecoration.aquifers, rOccupants);
    moisture_gen::RecomputeRainfall(*pWorld, rDecoration.moisture);
    ApplyBaseMoisture_(*pWorld);
    PlaceTileBonuses(*pWorld, rDecoration.tileBonuses, rOccupants, m_rng);

    return pWorld;
}

void WorldGenerator::GenerateElevation_(WorldMap& rWorld,
                                        const MapGenerationConfig_t& rConfig,
                                        const WorldGenPresetConfig_t& rPreset)
{
    // Drawn from m_rng, which the caller seeded: the map is a function of the one resolved
    // session seed, never of rConfig.seed (the request, where 0 means "pick one").
    const int noiseSeed = static_cast<int>(m_rng());
    const FbmNoise noise(noiseSeed, rPreset);
    const std::vector<float> field = SampleElevationField_(rWorld, noise, rPreset);
    const auto [minIt, maxIt] = std::ranges::minmax_element(field);
    const ElevationScale_t scale{*minIt, WaterThreshold_(field, rConfig.oceanCoverage), *maxIt};
    const int width = rWorld.GetWidth();
    for (const auto& pTile : rWorld.GetTiles())
    {
        const int tileIndex = TileIndex(pTile->GetX(), pTile->GetY(), width);
        const float value = field[static_cast<size_t>(tileIndex)];
        pTile->SetElevation(ElevationFor_(value, scale, rPreset));
    }
}

std::vector<float> WorldGenerator::SampleElevationField_(
    const WorldMap& rWorld, const FbmNoise& rNoise, const WorldGenPresetConfig_t& rPreset) const
{
    const int width = rWorld.GetWidth();
    const int height = rWorld.GetHeight();
    const float invWidth = width > 1 ? 1.0f / static_cast<float>(width - 1) : 0.0f;
    const float invHeight = height > 1 ? 1.0f / static_cast<float>(height - 1) : 0.0f;
    const float scale = std::max(rPreset.continentScale, 0.001f);
    // One map unit is √½ lattice lengths; sample on a cylinder so x wraps in noise space.
    constexpr float kTwoPi = 6.283185307179586f;
    constexpr float kSqrtHalf = 0.7071067811865476f;
    const float cylinderRadius =
        (static_cast<float>(width) * kSqrtHalf * scale) / kTwoPi;

    std::vector<float> field(rWorld.GetTiles().size());
    for (const auto& pTile : rWorld.GetTiles())
    {
        const int x = pTile->GetX();
        const int y = pTile->GetY();
        const float nx = static_cast<float>(x) * invWidth * 2.0f - 1.0f;
        const float ny = static_cast<float>(y) * invHeight * 2.0f - 1.0f;
        const float angle = (static_cast<float>(x) / static_cast<float>(width)) * kTwoPi;
        const float sampleX = std::cos(angle) * cylinderRadius;
        const float sampleZ = std::sin(angle) * cylinderRadius;
        const float sampleY = static_cast<float>(y) * kSqrtHalf * scale;
        const float sample = rNoise.Sample(sampleX, sampleY, sampleZ);
        const size_t index = static_cast<size_t>(TileIndex(x, y, width));
        field[index] = ApplyLandmassMask_(sample, nx, ny, rPreset);
    }
    return field;
}

float WorldGenerator::ApplyLandmassMask_(float noiseValue,
                                         float nx,
                                         float ny,
                                         const WorldGenPresetConfig_t& rPreset) const
{
    const float dist = std::sqrt(nx * nx + ny * ny);
    // Cylinder: only north/south are true edges; east/west wrap and must not be depressed.
    const float edge = std::abs(ny);
    float value = noiseValue;

    switch (rPreset.type)
    {
    case WorldGenPreset_t::Pangea:
    case WorldGenPreset_t::Continents:
    case WorldGenPreset_t::Islands:
    case WorldGenPreset_t::Archipelago:
        value += rPreset.centerBias * (1.0f - dist);
        value -= rPreset.edgeFalloff * edge;
        break;

    case WorldGenPreset_t::Ring:
    {
        // Peak landmass around mid-radius; depress map center and outer rim.
        constexpr float kRingRadius = 0.65f;
        const float ring = 1.0f - std::min(std::abs(dist - kRingRadius) * 2.5f, 1.0f);
        value += rPreset.centerBias * ring;
        value -= rPreset.edgeFalloff * edge;
        value -= rPreset.centerBias * 0.5f * std::max(0.0f, 1.0f - dist * 2.0f);
        break;
    }

    case WorldGenPreset_t::Lakes:
        // Prefer land near the rim and depress the interior for inland seas/lakes.
        value -= rPreset.centerBias * (1.0f - dist);
        value -= rPreset.edgeFalloff * edge * 0.5f;
        break;
    }

    return value;
}

void WorldGenerator::GenerateRockiness_(WorldMap& rWorld,
                                        ErosiveForces_t erosiveForces,
                                        const RockinessDecorationConfig_t& rRockiness)
{
    using namespace rockiness_gen;

    const RockinessWeights_t& rWeights = WeightsForLevel(rRockiness, erosiveForces);

    for (const auto& pTile : rWorld.GetTiles())
    {
        const Rockiness_t rockiness = SampleRockiness(rWeights, RandomFloat_());
        pTile->SetRockiness(rockiness);
    }
}

void WorldGenerator::GenerateAquifers_(WorldMap& rWorld,
                                       const AquiferDecorationConfig_t& rAquifers,
                                       const ImprovementRegistry& rOccupants)
{
    const ImprovementConfig_t& rAquifer = rOccupants.Get("Aquifer");
    std::vector<Tile*> candidates;
    for (auto& pTile : rWorld.GetTiles())
    {
        if (pTile && !pTile->GetHasAquifer() && CanBuildImprovement(*pTile, rAquifer, {}, {}))
        {
            candidates.push_back(pTile.get());
        }
    }

    if (!candidates.empty() && rAquifers.fraction > 0.0f)
    {
        std::shuffle(candidates.begin(), candidates.end(), m_rng);
        const float share = rAquifers.fraction * static_cast<float>(candidates.size());
        const size_t target = static_cast<size_t>(std::lround(share));
        const size_t count = std::min(target, candidates.size());
        for (size_t i = 0; i < count; ++i)
        {
            candidates[i]->SetHasAquifer(true);
        }
    }

    RecomputeRivers(rWorld);
}

float WorldGenerator::RandomFloat_()
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(m_rng);
}

} // namespace ac

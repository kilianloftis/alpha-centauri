#include "game/map/WorldGenerator.h"
#include "game/map/FbmNoise.h"
#include "game/map/FungusGeneration.h"
#include "game/map/TileBonusGeneration.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/LandmarkGeneration.h"
#include "game/map/MoistureGeneration.h"
#include "game/map/RiverGeneration.h"
#include "game/map/RockinessGeneration.h"

#include <algorithm>
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

} // namespace

WorldGenerator::WorldGenerator() = default;

WorldGenerator::~WorldGenerator() = default;

std::unique_ptr<WorldMap> WorldGenerator::Generate(const MapGenerationConfig_t& rConfig,
                                                   const WorldGenPresetConfig_t& rPreset,
                                                   const WorldGenDecorationConfig_t& rDecoration,
                                                   const std::vector<LandmarkConfig_t>& rLandmarks,
                                                   const ImprovementRegistry& rImprovements)
{
    const unsigned int seed = rConfig.seed == 0
        ? static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count())
        : rConfig.seed;
    m_rng.seed(seed);

    auto pWorld = std::make_unique<WorldMap>(rConfig.width, rConfig.height);

    GenerateElevation_(*pWorld, rConfig, rPreset);
    GenerateMoisture_(*pWorld, rDecoration.moisture);
    GenerateRockiness_(*pWorld, rConfig.erosiveForces, rDecoration.rockiness);
    GenerateAquifers_(*pWorld, rDecoration.aquifers);
    GenerateFungus_(*pWorld, rDecoration.fungus);
    GenerateLandmarks_(*pWorld, rLandmarks, rImprovements);
    GenerateTileBonuses_(*pWorld, rDecoration.tileBonuses, rImprovements);

    return pWorld;
}

void WorldGenerator::GenerateElevation_(WorldMap& rWorld,
                                        const MapGenerationConfig_t& rConfig,
                                        const WorldGenPresetConfig_t& rPreset)
{
    const int width = rWorld.GetWidth();
    const int height = rWorld.GetHeight();
    const int tileCount = width * height;
    if (tileCount <= 0)
    {
        return;
    }

    FbmNoise noise(static_cast<int>(rConfig.seed == 0 ? m_rng() : rConfig.seed), rPreset);

    const float invWidth = width > 1 ? 1.0f / static_cast<float>(width - 1) : 0.0f;
    const float invHeight = height > 1 ? 1.0f / static_cast<float>(height - 1) : 0.0f;
    const float scale = std::max(rPreset.continentScale, 0.001f);
    // Sample on a cylinder so x=0 and x=width are adjacent in noise space (horizontal wrap).
    constexpr float kTwoPi = 6.283185307179586f;
    const float cylinderRadius =
        (static_cast<float>(width) * scale) / kTwoPi;

    std::vector<float> field(static_cast<size_t>(tileCount));
    float minValue = 0.0f;
    float maxValue = 0.0f;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const float nx = static_cast<float>(x) * invWidth * 2.0f - 1.0f;
            const float ny = static_cast<float>(y) * invHeight * 2.0f - 1.0f;
            const float angle = (static_cast<float>(x) / static_cast<float>(width)) * kTwoPi;
            const float sampleX = std::cos(angle) * cylinderRadius;
            const float sampleZ = std::sin(angle) * cylinderRadius;
            const float sampleY = static_cast<float>(y) * scale;
            const float masked =
                ApplyLandmassMask_(noise.Sample(sampleX, sampleY, sampleZ), nx, ny, rPreset);

            const size_t index = static_cast<size_t>(y * width + x);
            field[index] = masked;
            if (index == 0 || masked < minValue)
            {
                minValue = masked;
            }
            if (index == 0 || masked > maxValue)
            {
                maxValue = masked;
            }
        }
    }

    float oceanCoverage = std::clamp(rConfig.oceanCoverage, 0.0f, 1.0f);
    size_t waterCount = static_cast<size_t>(oceanCoverage * static_cast<float>(tileCount));
    if (waterCount >= static_cast<size_t>(tileCount))
    {
        waterCount = static_cast<size_t>(tileCount) - 1;
    }

    std::vector<float> sorted = field;
    std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(waterCount),
                     sorted.end());
    const float threshold = sorted[waterCount];

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            Tile* pTile = rWorld.GetTile(x, y);
            if (!pTile)
            {
                continue;
            }

            const float value = field[static_cast<size_t>(y * width + x)];
            int elevation = 0;
            if (value < threshold)
            {
                elevation = static_cast<int>(std::lround(
                    Remap_(value, minValue, threshold,
                           static_cast<float>(rPreset.minElevation), -1.0f)));
            }
            else
            {
                elevation = static_cast<int>(std::lround(
                    Remap_(value, threshold, maxValue,
                           0.0f, static_cast<float>(rPreset.maxElevation))));
            }
            pTile->SetElevation(elevation);
        }
    }
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

void WorldGenerator::GenerateMoisture_(WorldMap& rWorld,
                                       const MoistureDecorationConfig_t& rMoisture)
{
    const int width = rWorld.GetWidth();
    const int height = rWorld.GetHeight();

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            Tile* pTile = rWorld.GetTile(x, y);
            if (!pTile)
            {
                continue;
            }

            // Mid-band base so unaided tiles still produce a mix of tiers.
            float score = rMoisture.baseMin + RandomFloat_() * rMoisture.baseRange;
            score += moisture_gen::TropicalMoistureBonus(y, height, rMoisture);

            if (pTile->IsLand())
            {
                score += moisture_gen::CoastalMoistureBonus(*pTile, rWorld, rMoisture);

                const Tile* pWest = rWorld.GetTile(x - 1, y);
                const Tile* pEast = rWorld.GetTile(x + 1, y);
                const int elevWest = pWest ? pWest->GetElevation() : pTile->GetElevation();
                const int elevEast = pEast ? pEast->GetElevation() : pTile->GetElevation();
                score += moisture_gen::OrographicMoistureBias(
                    pTile->GetElevation(), elevWest, elevEast, rMoisture);
            }

            const Moisture_t moisture = moisture_gen::QuantizeMoistureScore(score, rMoisture);
            pTile->SetBaseMoisture(moisture);
            pTile->SetMoisture(moisture);
        }
    }
}

void WorldGenerator::GenerateRockiness_(WorldMap& rWorld,
                                        ErosiveForces_t erosiveForces,
                                        const RockinessDecorationConfig_t& rRockiness)
{
    using namespace rockiness_gen;

    const RockinessWeights_t& rWeights = WeightsForLevel(rRockiness, erosiveForces);

    for (int y = 0; y < rWorld.GetHeight(); ++y)
    {
        for (int x = 0; x < rWorld.GetWidth(); ++x)
        {
            Tile* pTile = rWorld.GetTile(x, y);
            if (pTile)
            {
                pTile->SetRockiness(SampleRockiness(rWeights, RandomFloat_()));
            }
        }
    }
}

void WorldGenerator::GenerateAquifers_(WorldMap& rWorld,
                                       const AquiferDecorationConfig_t& rAquifers)
{
    std::vector<Tile*> landTiles;
    for (auto& pTile : rWorld.GetTiles())
    {
        if (pTile && pTile->IsLand())
        {
            landTiles.push_back(pTile.get());
        }
    }

    if (!landTiles.empty() && rAquifers.landFraction > 0.0f)
    {
        std::shuffle(landTiles.begin(), landTiles.end(), m_rng);
        const size_t target = static_cast<size_t>(
            std::lround(rAquifers.landFraction * static_cast<float>(landTiles.size())));
        const size_t count = std::min(target, landTiles.size());
        for (size_t i = 0; i < count; ++i)
        {
            landTiles[i]->SetHasAquifer(true);
        }
    }

    RecomputeRivers(rWorld);
}

void WorldGenerator::GenerateFungus_(WorldMap& rWorld, const FungusDecorationConfig_t& rFungus)
{
    PlaceFungus(rWorld, rFungus, m_rng);
}

void WorldGenerator::GenerateLandmarks_(WorldMap& rWorld,
                                        const std::vector<LandmarkConfig_t>& rLandmarks,
                                        const ImprovementRegistry& rImprovements)
{
    PlaceLandmarks(rWorld, rLandmarks, rImprovements, m_rng);
}

void WorldGenerator::GenerateTileBonuses_(WorldMap& rWorld,
                                          const TileBonusDecorationConfig_t& rBonuses,
                                          const ImprovementRegistry& rImprovements)
{
    PlaceTileBonuses(rWorld, rBonuses, rImprovements, m_rng);
}

int WorldGenerator::RandomInt_(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(m_rng);
}

float WorldGenerator::RandomFloat_()
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(m_rng);
}

} // namespace ac

#include "game/worldgen/FungusGeneration.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <FastNoiseLite.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace ac
{

namespace
{

constexpr float k_MinFill_ = 1.0e-3f;
constexpr float k_FineScaleFactor_ = 4.0f;

class ScaledNoise_
{
public:
    ScaledNoise_(int seed, float frequency)
    {
        m_noise.SetSeed(seed);
        m_noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        m_noise.SetFrequency(frequency);
    }

    float Sample(float x, float y) const
    {
        return m_noise.GetNoise(x, y);
    }

private:
    FastNoiseLite m_noise;
};

struct FungusCandidate_t
{
    Tile* pTile = nullptr;
    float coarse = 0.0f;
    float fine = 0.0f;
};

bool IsEligible_(const Tile& rTile, const ImprovementConfig_t& rFungus)
{
    if (rTile.HasTerrainFeature(rFungus.id))
    {
        return false;
    }
    return CanBuildImprovement(rTile, rFungus, {}, {});
}

float ClampedCoverage_(float coverage, float multiplier)
{
    return std::clamp(coverage * multiplier, 0.0f, 1.0f);
}

float ClampedFill_(float fill)
{
    return std::clamp(fill, k_MinFill_, 1.0f);
}

std::vector<FungusCandidate_t> CollectCandidates_(WorldMap& rWorld,
                                                  const ImprovementConfig_t& rFungus,
                                                  const ScaledNoise_& rCoarse,
                                                  const ScaledNoise_& rFine)
{
    std::vector<FungusCandidate_t> candidates;
    for (const std::unique_ptr<Tile>& pOwned : rWorld.GetTiles())
    {
        Tile& rTile = *pOwned;
        if (!IsEligible_(rTile, rFungus))
        {
            continue;
        }
        const float x = static_cast<float>(rTile.GetX());
        const float y = static_cast<float>(rTile.GetY());
        candidates.push_back(
            FungusCandidate_t{&rTile, rCoarse.Sample(x, y), rFine.Sample(x, y)});
    }
    return candidates;
}

size_t CountForFraction_(size_t total, float fraction)
{
    if (total == 0 || fraction <= 0.0f)
    {
        return 0;
    }
    const double share = static_cast<double>(fraction) * static_cast<double>(total);
    const size_t rounded = static_cast<size_t>(std::lround(share));
    return std::min(total, std::max<size_t>(rounded, fraction > 0.0f ? 1 : 0));
}

void StampTopByScore_(std::vector<FungusCandidate_t>& rFootprint, size_t keepCount, bool bUseFine,
                      const ImprovementConfig_t& rFungus)
{
    if (keepCount == 0 || rFootprint.empty())
    {
        return;
    }
    const size_t count = std::min(keepCount, rFootprint.size());
    const auto byScore = [bUseFine](const FungusCandidate_t& rLeft, const FungusCandidate_t& rRight)
    {
        const float left = bUseFine ? rLeft.fine : rLeft.coarse;
        const float right = bUseFine ? rRight.fine : rRight.coarse;
        return left > right;
    };
    std::partial_sort(rFootprint.begin(), rFootprint.begin() + static_cast<std::ptrdiff_t>(count),
                      rFootprint.end(), byScore);
    for (size_t i = 0; i < count; ++i)
    {
        rFootprint[i].pTile->AddTerrainFeature(rFungus);
    }
}

void PlaceFromCoverage_(std::vector<FungusCandidate_t>& rCandidates, float coverage, float fill,
                        const ImprovementConfig_t& rFungus)
{
    const size_t eligible = rCandidates.size();
    const size_t keepCount = CountForFraction_(eligible, coverage);
    if (keepCount == 0)
    {
        return;
    }

    size_t footprintCount = CountForFraction_(eligible, std::min(1.0f, coverage / fill));
    footprintCount = std::max(footprintCount, keepCount);

    std::partial_sort(rCandidates.begin(),
                      rCandidates.begin() + static_cast<std::ptrdiff_t>(footprintCount),
                      rCandidates.end(),
                      [](const FungusCandidate_t& rLeft, const FungusCandidate_t& rRight)
                      {
                          return rLeft.coarse > rRight.coarse;
                      });
    std::vector<FungusCandidate_t> footprint(rCandidates.begin(),
                                             rCandidates.begin()
                                                 + static_cast<std::ptrdiff_t>(footprintCount));
    const bool bUseFine = fill < 1.0f;
    StampTopByScore_(footprint, keepCount, bUseFine, rFungus);
}

bool PoleRowRoll_(const Tile& rTile, int mapHeight, float chance, std::mt19937& rRng)
{
    if (rTile.GetY() != 0 && rTile.GetY() != mapHeight - 1)
    {
        return false;
    }
    if (chance <= 0.0f)
    {
        return false;
    }
    if (chance >= 1.0f)
    {
        return true;
    }
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    return unit(rRng) < chance;
}

void RollPoleRows_(WorldMap& rWorld, const FungusDecorationConfig_t& rConfig,
                   const ImprovementConfig_t& rFungus, std::mt19937& rRng)
{
    const int height = rWorld.GetHeight();
    for (const std::unique_ptr<Tile>& pOwned : rWorld.GetTiles())
    {
        Tile& rTile = *pOwned;
        if (rTile.HasTerrainFeature(rFungus.id) || !IsEligible_(rTile, rFungus))
        {
            continue;
        }
        if (PoleRowRoll_(rTile, height, rConfig.poleRowChance, rRng))
        {
            rTile.AddTerrainFeature(rFungus);
        }
    }
}

} // namespace

void PlaceFungus(WorldMap& rWorld, const FungusDecorationConfig_t& rConfig,
                 float coverageMultiplier, const ImprovementConfig_t& rFungus,
                 std::mt19937& rRng)
{
    const float coverage = ClampedCoverage_(rConfig.coverage, coverageMultiplier);
    const float fill = ClampedFill_(rConfig.fill);
    const float clusterScale = static_cast<float>(rConfig.clusterScaleTiles);
    const float coarseFrequency = 1.0f / clusterScale;
    const float fineFrequency = k_FineScaleFactor_ / clusterScale;

    const int coarseSeed = static_cast<int>(rRng());
    const int fineSeed = static_cast<int>(rRng());
    const ScaledNoise_ coarse(coarseSeed, coarseFrequency);
    const ScaledNoise_ fine(fineSeed, fineFrequency);

    std::vector<FungusCandidate_t> candidates =
        CollectCandidates_(rWorld, rFungus, coarse, fine);
    PlaceFromCoverage_(candidates, coverage, fill, rFungus);
    RollPoleRows_(rWorld, rConfig, rFungus, rRng);
}

} // namespace ac

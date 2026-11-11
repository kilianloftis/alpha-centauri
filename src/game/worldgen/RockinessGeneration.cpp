#include "game/worldgen/RockinessGeneration.h"

#include <algorithm>
#include <stdexcept>

namespace ac
{

const RockinessBand_t& BandForElevation(const RockinessDecorationConfig_t& rConfig,
                                        int elevationAboveOcean)
{
    for (const RockinessBand_t& rBand : rConfig.bands)
    {
        if (!rBand.maxElevationAboveOceanMeters.has_value())
        {
            return rBand;
        }
        if (elevationAboveOcean < *rBand.maxElevationAboveOceanMeters)
        {
            return rBand;
        }
    }
    throw std::runtime_error("rockiness bands must end with an unbounded top tier");
}

Rockiness_t SampleRockiness(const RockinessWeights_t& rWeights, float u)
{
    u = std::clamp(u, 0.0f, 1.0f);
    const float flat = std::max(0.0f, rWeights.flat);
    const float rolling = std::max(0.0f, rWeights.rolling);
    const float rocky = std::max(0.0f, rWeights.rocky);
    const float total = flat + rolling + rocky;
    if (total <= 0.0f)
    {
        throw std::runtime_error("Invalid rockiness weights");
    }

    const float flatEnd = flat / total;
    const float rollingEnd = flatEnd + rolling / total;
    if (u < flatEnd)
    {
        return Rockiness_t::Flat;
    }
    if (u < rollingEnd)
    {
        return Rockiness_t::Rolling;
    }
    return Rockiness_t::Rocky;
}

void PlaceRockiness(WorldMap& rWorld, const RockinessDecorationConfig_t& rConfig,
                    std::mt19937& rRng)
{
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const int oceanLevel = rWorld.GetTiles().front()->MapRules().oceanLevelMeters;
    const int height = rWorld.GetHeight();
    for (int y = 0; y < height; ++y)
    {
        for (int x = (y & 1); x < rWorld.GetWidth(); x += 2)
        {
            Tile& rTile = *rWorld.GetTile(x, y);
            const bool bPole = (y == 0 || y == height - 1);
            const RockinessWeights_t& rWeights =
                bPole ? rConfig.poles
                      : BandForElevation(rConfig, rTile.GetElevation() - oceanLevel).weights;
            rTile.SetRockiness(SampleRockiness(rWeights, unit(rRng)));
        }
    }
}

} // namespace ac

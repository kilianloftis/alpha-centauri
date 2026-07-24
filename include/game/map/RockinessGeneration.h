#pragma once

#include "game/map/MapGenerationConfig.h"
#include "game/map/Tile.h"
#include "game/map/WorldGenDecorationConfig.h"

#include <algorithm>
#include <stdexcept>

namespace ac
{
namespace rockiness_gen
{

inline const RockinessWeights_t& WeightsForLevel(const RockinessDecorationConfig_t& rConfig,
                                                 ErosiveForces_t level)
{
    switch (level)
    {
    case ErosiveForces_t::Low:
        return rConfig.low;
    case ErosiveForces_t::Average:
        return rConfig.average;
    case ErosiveForces_t::High:
        return rConfig.high;
    }
    throw std::runtime_error("Unknown erosive forces level");
}

// u in [0, 1). Weights are expected non-negative; zero-sum falls back to Flat.
inline Rockiness_t SampleRockiness(const RockinessWeights_t& rWeights, float u)
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

} // namespace rockiness_gen
} // namespace ac

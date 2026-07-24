#pragma once

#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldGenDecorationConfig.h"
#include "game/map/WorldMap.h"

#include <algorithm>

namespace ac
{
namespace moisture_gen
{

// Slight coastal humidity: Chebyshev distance to nearest water within coastalRadius.
// Land only; falls off with distance (peak at adjacent).
inline float CoastalMoistureBonus(const Tile& rTile,
                                  const WorldMap& rWorld,
                                  const MoistureDecorationConfig_t& rConfig)
{
    if (!rTile.IsLand())
    {
        return 0.0f;
    }

    int nearestWaterDist = rConfig.coastalRadius + 1;
    ForEachTileInChebyshevRadius(rTile, rWorld, rConfig.coastalRadius, /*includeOrigin=*/false,
        [&](const Tile* pNeighbor, int distance)
        {
            if (pNeighbor->IsWater() && distance < nearestWaterDist)
            {
                nearestWaterDist = distance;
            }
        });

    if (nearestWaterDist > rConfig.coastalRadius)
    {
        return 0.0f;
    }

    // dist 1 → full peak; farther → linearly less (for radius R).
    return rConfig.coastalPeakBonus *
           (static_cast<float>(rConfig.coastalRadius + 1 - nearestWaterDist) /
            static_cast<float>(rConfig.coastalRadius));
}

// Equator (mid-map Y) is wetter; falls off smoothly inside the tropical band.
inline float TropicalMoistureBonus(int y, int height, const MoistureDecorationConfig_t& rConfig)
{
    if (height <= 1)
    {
        return rConfig.tropicalPeakBonus;
    }

    const float ny = static_cast<float>(y) / static_cast<float>(height - 1) * 2.0f - 1.0f;
    const float absLat = std::abs(ny);
    if (absLat >= rConfig.tropicalHalfWidth)
    {
        return 0.0f;
    }
    return rConfig.tropicalPeakBonus * (1.0f - absLat / rConfig.tropicalHalfWidth);
}

// Western slopes (rising toward the east) are wetter; eastern slopes more arid.
// Scaled by local elevation so mountain faces matter more than flat plains. Water → 0.
inline float OrographicMoistureBias(int localElev,
                                    int elevWest,
                                    int elevEast,
                                    const MoistureDecorationConfig_t& rConfig)
{
    if (localElev < 0)
    {
        return 0.0f;
    }

    const float grad = static_cast<float>(elevEast - elevWest);
    const float clampedGrad =
        std::clamp(grad / rConfig.orographicElevScale, -1.0f, 1.0f);
    const float elevWeight =
        std::clamp(static_cast<float>(localElev) / rConfig.orographicMaxElev, 0.0f, 1.0f);
    return rConfig.orographicStrength * clampedGrad * elevWeight;
}

inline Moisture_t QuantizeMoistureScore(float score, const MoistureDecorationConfig_t& rConfig)
{
    score = std::clamp(score, 0.0f, 1.0f);
    if (score < rConfig.aridThreshold)
    {
        return Moisture_t::Arid;
    }
    if (score < rConfig.moistThreshold)
    {
        return Moisture_t::Moist;
    }
    return Moisture_t::Wet;
}

} // namespace moisture_gen
} // namespace ac

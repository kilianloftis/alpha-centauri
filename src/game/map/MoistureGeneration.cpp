#include "game/map/MoistureGeneration.h"

#include "game/map/MapUtils.h"
#include "game/map/Tile.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace ac
{
namespace moisture_gen
{
namespace
{

Moisture_t MoistureFromTier_(int tier)
{
    switch (std::clamp(tier, 0, 2))
    {
    case 0:
        return Moisture_t::Arid;
    case 1:
        return Moisture_t::Moist;
    default:
        return Moisture_t::Wet;
    }
}

int CloudCover_(Rainfall_t rainfall)
{
    switch (rainfall)
    {
    case Rainfall_t::Arid:
        return 0;
    case Rainfall_t::Average:
        return 1;
    case Rainfall_t::Wet:
        return 2;
    }
    throw std::invalid_argument("Unknown rainfall value");
}

void SetTileMoisture_(Tile& rTile, int tier)
{
    const Moisture_t moisture = MoistureFromTier_(tier);
    rTile.SetBaseMoisture(moisture);
    rTile.SetMoisture(moisture);
}

} // namespace

int AltitudeBand(int elevationMeters,
                 int oceanLevelMeters,
                 const MoistureDecorationConfig_t& rConfig)
{
    if (rConfig.hillMinElevationMeters <= 0
        || rConfig.peakMinElevationMeters <= rConfig.hillMinElevationMeters)
    {
        throw std::invalid_argument(
            "rainfall altitude bands require 0 < hill_min < peak_min");
    }

    if (elevationMeters < oceanLevelMeters)
    {
        const int bandStep = rConfig.peakMinElevationMeters - rConfig.hillMinElevationMeters;
        const int depth = oceanLevelMeters - elevationMeters;
        if (depth >= 2 * bandStep)
        {
            return 0;
        }
        if (depth >= bandStep)
        {
            return 1;
        }
        return k_AltOceanShelf;
    }

    const int aboveSea = elevationMeters - oceanLevelMeters;
    const int bandStep = rConfig.peakMinElevationMeters - rConfig.hillMinElevationMeters;
    const int oneAboveMin = rConfig.hillMinElevationMeters - bandStep;
    if (aboveSea < oneAboveMin)
    {
        return k_AltShoreLine;
    }
    if (aboveSea < rConfig.hillMinElevationMeters)
    {
        return k_AltOneAboveSea;
    }
    if (aboveSea < rConfig.peakMinElevationMeters)
    {
        return k_AltTwoAboveSea;
    }
    if (aboveSea < rConfig.peakMinElevationMeters + bandStep)
    {
        return k_AltThreeAboveSea;
    }
    return k_AltFourAboveSea;
}

int StubTemperature(int y, int mapHeight)
{
    if (mapHeight <= 1)
    {
        return 2;
    }
    const float absLat =
        std::abs(static_cast<float>(y) / static_cast<float>(mapHeight - 1) * 2.0f - 1.0f);
    if (absLat < 1.0f / 3.0f)
    {
        return 2;
    }
    if (absLat < 2.0f / 3.0f)
    {
        return 1;
    }
    return 0;
}

void GenerateRainfall(WorldMap& rWorld,
                      const MoistureDecorationConfig_t& rConfig,
                      Rainfall_t rainfall)
{
    const int width = rWorld.GetWidth();
    const int height = rWorld.GetHeight();
    const int tileCount = static_cast<int>(rWorld.GetTiles().size());
    const int cloudCover = CloudCover_(rainfall);
    const int oceanLevel = rWorld.GetTiles().front()->MapRules().oceanLevelMeters;

    std::vector<int> alts(static_cast<std::size_t>(tileCount));
    std::vector<int> temps(static_cast<std::size_t>(tileCount));
    std::vector<int> flags(static_cast<std::size_t>(tileCount), 0);

    for (const auto& pOwnedTile : rWorld.GetTiles())
    {
        Tile& rTile = *pOwnedTile;
        const int idx = rWorld.GetTileIndex(rTile);
        alts[static_cast<std::size_t>(idx)] =
            AltitudeBand(rTile.GetElevation(), oceanLevel, rConfig);
        temps[static_cast<std::size_t>(idx)] = StubTemperature(rTile.GetY(), height);
    }

    // Mark pass: ocean moisture eastward + cloudmass west-wet / east-dry belts.
    for (int y = 0; y < height; ++y)
    {
        for (int x = (y & 1); x < width; x += 2)
        {
            Tile* pTile = rWorld.GetTile(x, y);
            if (!pTile)
            {
                continue;
            }
            const int idx = rWorld.GetTileIndex(*pTile);
            const int alt = alts[static_cast<std::size_t>(idx)];

            if (pTile->IsWater())
            {
                for (int i = 0, px = x; i < 3; ++i, px -= 2)
                {
                    Tile* pWest = rWorld.GetTile(px - 2, y);
                    Tile* pCur = rWorld.GetTile(px, y);
                    if (pWest && pCur
                        && alts[static_cast<std::size_t>(rWorld.GetTileIndex(*pWest))]
                               >= alts[static_cast<std::size_t>(rWorld.GetTileIndex(*pCur))]
                        && i <= cloudCover)
                    {
                        flags[static_cast<std::size_t>(rWorld.GetTileIndex(*pWest))] &=
                            ~k_FlagLeeDry;
                    }
                }
                if (Tile* pEast = rWorld.GetTile(x + 2, y))
                {
                    flags[static_cast<std::size_t>(rWorld.GetTileIndex(*pEast))] |=
                        k_FlagOceanMoisture;
                }
                if (cloudCover > 0)
                {
                    if (Tile* pEast2 = rWorld.GetTile(x + 4, y))
                    {
                        const int e2 = rWorld.GetTileIndex(*pEast2);
                        if (temps[static_cast<std::size_t>(e2)] > 1 || cloudCover > 1)
                        {
                            flags[static_cast<std::size_t>(e2)] |= k_FlagOceanMoisture;
                            if (cloudCover > 1)
                            {
                                if (Tile* pEast3 = rWorld.GetTile(x + 6, y))
                                {
                                    const int e3 = rWorld.GetTileIndex(*pEast3);
                                    if (temps[static_cast<std::size_t>(e3)] > 0)
                                    {
                                        flags[static_cast<std::size_t>(e3)] |=
                                            k_FlagOceanMoisture;
                                    }
                                }
                            }
                        }
                    }
                }
                continue;
            }

            // TODO: forest humidity (0x40) when climate is re-run after forests exist.
            if (alt <= k_AltOneAboveSea)
            {
                continue;
            }

            const int cloudmass =
                (alt <= k_AltTwoAboveSea) ? rConfig.cloudmassHills : rConfig.cloudmassPeaks;
            const int rainfallDelta = rConfig.rainfallCoeff * (cloudCover - 1);
            const int maxBackSteps = cloudmass + rainfallDelta;
            if (maxBackSteps >= 1)
            {
                for (int step = 1, bx = x - 2; step <= maxBackSteps; ++step, bx -= 2)
                {
                    Tile* pBack = rWorld.GetTile(bx, y);
                    if (!pBack || pBack->IsWater()
                        || alts[static_cast<std::size_t>(rWorld.GetTileIndex(*pBack))] >= alt)
                    {
                        break;
                    }
                    flags[static_cast<std::size_t>(rWorld.GetTileIndex(*pBack))] |=
                        k_FlagWindwardWet;
                }
            }
            const int maxFwdSteps = cloudmass - rainfallDelta;
            if (maxFwdSteps >= 1)
            {
                for (int step = 1, fx = x + 2; step <= maxFwdSteps; ++step, fx += 2)
                {
                    Tile* pFwd = rWorld.GetTile(fx, y);
                    if (!pFwd || pFwd->IsWater()
                        || alts[static_cast<std::size_t>(rWorld.GetTileIndex(*pFwd))] >= alt)
                    {
                        break;
                    }
                    flags[static_cast<std::size_t>(rWorld.GetTileIndex(*pFwd))] |= k_FlagLeeDry;
                }
            }
        }
    }

    // Row scan: stateful rain belts → discrete Arid/Moist/Wet.
    // Condenser bits stay in RecomputeMoisture; forest humidity omitted (no world-gen forests).
    int rainSteps = 0;
    int rainFlag = 0;
    int drySteps = 0;

    for (int y = 0; y < height; ++y)
    {
        for (int x = (y & 1); x < width; x += 2)
        {
            Tile* pTile = rWorld.GetTile(x, y);
            if (!pTile)
            {
                continue;
            }
            if (y <= 0 || y >= height - 1)
            {
                SetTileMoisture_(*pTile, 0);
                continue;
            }

            const int idx = rWorld.GetTileIndex(*pTile);
            const int alt = alts[static_cast<std::size_t>(idx)];
            const int temperature = temps[static_cast<std::size_t>(idx)];
            const int tileFlags = flags[static_cast<std::size_t>(idx)];
            int moistureMod = 0;
            int moisture = 0;
            int sloping = 0;

            // SMAC mesa / dunes seed rain_flag at the start of the tile.
            if (pTile->HasFeature("SunnyMesa") || pTile->HasFeature("GreatDunes"))
            {
                rainFlag = 1;
            }

            if (tileFlags & k_FlagWindwardWet)
            {
                moisture = 1;
                rainFlag = 1;
                drySteps = 0;
                sloping = 1;
            }
            if (tileFlags & k_FlagOceanMoisture)
            {
                rainFlag = (temperature > 1) ? 1 : 0;
                rainSteps = 0;
                drySteps = 0;
                if (cloudCover >= 1)
                {
                    moistureMod = 1;
                }
            }
            if (tileFlags & k_FlagLeeDry)
            {
                if (rainFlag)
                {
                    --rainFlag;
                }
                --moistureMod;
            }

            if (alt > k_AltShoreLine)
            {
                if (++rainSteps > 4 * cloudCover * rConfig.rainfallCoeff + 1 && rainFlag)
                {
                    --rainFlag;
                }
                moisture = sloping;
            }
            else
            {
                bool bResetMoist = false;
                if (alt == k_AltShoreLine)
                {
                    if (rainFlag)
                    {
                        bResetMoist = true;
                        if (rainSteps == 0 && ++drySteps > 4 * cloudCover + 2)
                        {
                            rainFlag = 0;
                            drySteps = 0;
                        }
                    }
                }
                else
                {
                    rainFlag = 1;
                    drySteps = 0;
                }
                if (rainSteps > 0)
                {
                    bResetMoist = true;
                    rainSteps = rainSteps - 1 - cloudCover;
                    if (rainSteps < 1)
                    {
                        rainSteps = 0;
                        drySteps = 0;
                        if (cloudCover > 0 || alt < k_AltShoreLine)
                        {
                            rainFlag = 1;
                        }
                    }
                }
                if (bResetMoist)
                {
                    moisture = sloping;
                }
            }

            if (pTile->GetHasRiver())
            {
                // SMAC: every third column keeps a negative moisture_mod; others clear it.
                if (((x >> 1) % 3) != 0 && moistureMod < 0)
                {
                    moistureMod = 0;
                }
                rainFlag = 1;
                drySteps = 0;
            }

            if (rainFlag)
            {
                moistureMod += rainFlag;
            }
            if (alt <= (temperature != 0) + k_AltOneAboveSea)
            {
                moisture += moistureMod;
            }
            else if (cloudCover - alt + temperature + 5 >= 2 && moistureMod > 0)
            {
                ++moisture;
            }

            // Monsoon Jungle: SMAC boosts land below peak band (no per-tile landmark code).
            if (pTile->HasFeature("MonsoonJungle") && pTile->IsLand()
                && alt < k_AltThreeAboveSea)
            {
                moisture += 2;
            }
            if (pTile->HasFeature("GreatDunes") || pTile->HasFeature("UnityWreckage"))
            {
                moisture = 0;
            }

            if (moisture < 0)
            {
                moisture = 0;
            }
            else if (moisture > 2)
            {
                moisture = 2;
            }
            SetTileMoisture_(*pTile, moisture);
        }
    }

    // Neighbor smooth: arid land next to rainy land becomes moist.
    for (int y = 0; y < height; ++y)
    {
        for (int x = (y & 1); x < width; x += 2)
        {
            Tile* pTile = rWorld.GetTile(x, y);
            if (!pTile || pTile->IsWater())
            {
                continue;
            }
            if (pTile->GetMoisture() != Moisture_t::Arid)
            {
                continue;
            }
            for (const LatticeOffset_t& rOff : k_RingNeighbors)
            {
                Tile* pNeighbor = GetTileAtLatticeOffset(rWorld, *pTile, rOff.p, rOff.q);
                if (pNeighbor && pNeighbor->IsLand()
                    && pNeighbor->GetMoisture() == Moisture_t::Wet)
                {
                    SetTileMoisture_(*pTile, 1);
                    break;
                }
            }
        }
    }
}

} // namespace moisture_gen
} // namespace ac

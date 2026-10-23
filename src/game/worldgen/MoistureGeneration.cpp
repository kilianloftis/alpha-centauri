#include "game/worldgen/MoistureGeneration.h"

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

namespace
{

// Per-tile altitude band, stub temperature and mark-pass flags, indexed by tile index.
struct RainfallGrids_t
{
    std::vector<int> alts;
    std::vector<int> temps;
    std::vector<int> flags;

    int& Flags(const WorldMap& rWorld, const Tile& rTile)
    {
        return flags[static_cast<std::size_t>(rWorld.GetTileIndex(rTile))];
    }
    int Alt(const WorldMap& rWorld, const Tile& rTile) const
    {
        return alts[static_cast<std::size_t>(rWorld.GetTileIndex(rTile))];
    }
    int Temp(const WorldMap& rWorld, const Tile& rTile) const
    {
        return temps[static_cast<std::size_t>(rWorld.GetTileIndex(rTile))];
    }
};

RainfallGrids_t SampleTiles_(const WorldMap& rWorld, const MoistureDecorationConfig_t& rConfig)
{
    const std::size_t tileCount = rWorld.GetTiles().size();
    const int oceanLevel = rWorld.GetTiles().front()->MapRules().oceanLevelMeters;
    RainfallGrids_t grids{std::vector<int>(tileCount), std::vector<int>(tileCount),
                          std::vector<int>(tileCount, 0)};
    for (const auto& pOwnedTile : rWorld.GetTiles())
    {
        const Tile& rTile = *pOwnedTile;
        const auto idx = static_cast<std::size_t>(rWorld.GetTileIndex(rTile));
        grids.alts[idx] = AltitudeBand(rTile.GetElevation(), oceanLevel, rConfig);
        grids.temps[idx] = StubTemperature(rTile.GetY(), rWorld.GetHeight());
    }
    return grids;
}

// Visits every tile once in SMAC's row order (odd rows start at x = 1).
template <typename VisitFn>
void ForEachRowTile_(WorldMap& rWorld, VisitFn visit)
{
    for (int y = 0; y < rWorld.GetHeight(); ++y)
    {
        for (int x = (y & 1); x < rWorld.GetWidth(); x += 2)
        {
            if (Tile* pTile = rWorld.GetTile(x, y))
            {
                visit(*pTile, x, y);
            }
        }
    }
}

// Water clears lee dryness on up to cloudCover + 1 tiles to its west that do not climb.
void ClearLeeDryUpwind_(const WorldMap& rWorld, RainfallGrids_t& rGrids, int x, int y,
                        int cloudCover)
{
    for (int i = 0, px = x; i < 3; ++i, px -= 2)
    {
        const Tile* pWest = rWorld.GetTile(px - 2, y);
        const Tile* pCur = rWorld.GetTile(px, y);
        if (pWest && pCur && rGrids.Alt(rWorld, *pWest) >= rGrids.Alt(rWorld, *pCur)
            && i <= cloudCover)
        {
            rGrids.Flags(rWorld, *pWest) &= ~k_FlagLeeDry;
        }
    }
}

// Ocean moisture advects east: one tile always, a second when warm or wet, a third when wet.
void MarkOceanMoistureEast_(const WorldMap& rWorld, RainfallGrids_t& rGrids, int x, int y,
                            int cloudCover)
{
    if (const Tile* pEast = rWorld.GetTile(x + 2, y))
    {
        rGrids.Flags(rWorld, *pEast) |= k_FlagOceanMoisture;
    }
    if (cloudCover <= 0)
    {
        return;
    }
    const Tile* pEast2 = rWorld.GetTile(x + 4, y);
    if (!pEast2 || (rGrids.Temp(rWorld, *pEast2) <= 1 && cloudCover <= 1))
    {
        return;
    }
    rGrids.Flags(rWorld, *pEast2) |= k_FlagOceanMoisture;
    const Tile* pEast3 = rWorld.GetTile(x + 6, y);
    if (cloudCover > 1 && pEast3 && rGrids.Temp(rWorld, *pEast3) > 0)
    {
        rGrids.Flags(rWorld, *pEast3) |= k_FlagOceanMoisture;
    }
}

// Marks up to maxSteps lower land tiles in direction dx from the highland at x with flag.
void MarkBelt_(const WorldMap& rWorld, RainfallGrids_t& rGrids, int x, int y, int alt, int dx,
               int maxSteps, int flag)
{
    for (int step = 1, bx = x + dx; step <= maxSteps; ++step, bx += dx)
    {
        const Tile* pTile = rWorld.GetTile(bx, y);
        if (!pTile || pTile->IsWater() || rGrids.Alt(rWorld, *pTile) >= alt)
        {
            return;
        }
        rGrids.Flags(rWorld, *pTile) |= flag;
    }
}

// Highland traps cloudmass: windward (west) tiles get wet, lee (east) tiles get dry, with
// Rainfall lengthening the west belt and shortening the east one.
void MarkCloudmass_(const WorldMap& rWorld, RainfallGrids_t& rGrids,
                    const MoistureDecorationConfig_t& rConfig, int x, int y, int alt,
                    int cloudCover)
{
    const int cloudmass =
        (alt <= k_AltTwoAboveSea) ? rConfig.cloudmassHills : rConfig.cloudmassPeaks;
    const int rainfallDelta = rConfig.rainfallCoeff * (cloudCover - 1);
    MarkBelt_(rWorld, rGrids, x, y, alt, -2, cloudmass + rainfallDelta, k_FlagWindwardWet);
    MarkBelt_(rWorld, rGrids, x, y, alt, 2, cloudmass - rainfallDelta, k_FlagLeeDry);
}

// Ocean moisture eastward + cloudmass west-wet / east-dry belts.
void MarkPass_(WorldMap& rWorld, RainfallGrids_t& rGrids, const MoistureDecorationConfig_t& rConfig,
               int cloudCover)
{
    ForEachRowTile_(rWorld, [&](Tile& rTile, int x, int y) {
        if (rTile.IsWater())
        {
            ClearLeeDryUpwind_(rWorld, rGrids, x, y, cloudCover);
            MarkOceanMoistureEast_(rWorld, rGrids, x, y, cloudCover);
            return;
        }
        // TODO: forest humidity (0x40) when climate is re-run after forests exist.
        const int alt = rGrids.Alt(rWorld, rTile);
        if (alt > k_AltOneAboveSea)
        {
            MarkCloudmass_(rWorld, rGrids, rConfig, x, y, alt, cloudCover);
        }
    });
}

// The rain belt carried along the row scan, from tile to tile and row to row.
struct RainBelt_t
{
    int rainSteps = 0;
    int rainFlag = 0;
    int drySteps = 0;
};

// One tile's working values in the row scan.
struct TileRain_t
{
    int moisture = 0;
    int moistureMod = 0;
    int sloping = 0;
};

void ApplyMarkFlags_(RainBelt_t& rBelt, TileRain_t& rRain, int tileFlags, int temperature,
                     int cloudCover)
{
    if (tileFlags & k_FlagWindwardWet)
    {
        rRain.moisture = 1;
        rBelt.rainFlag = 1;
        rBelt.drySteps = 0;
        rRain.sloping = 1;
    }
    if (tileFlags & k_FlagOceanMoisture)
    {
        rBelt.rainFlag = (temperature > 1) ? 1 : 0;
        rBelt.rainSteps = 0;
        rBelt.drySteps = 0;
        if (cloudCover >= 1)
        {
            rRain.moistureMod = 1;
        }
    }
    if (tileFlags & k_FlagLeeDry)
    {
        if (rBelt.rainFlag)
        {
            --rBelt.rainFlag;
        }
        --rRain.moistureMod;
    }
}

// Shore and sea tiles keep the belt going or end it; returns whether moisture resets to sloping.
bool AdvanceLowlandBelt_(RainBelt_t& rBelt, int alt, int cloudCover)
{
    bool bResetMoist = false;
    if (alt == k_AltShoreLine)
    {
        if (rBelt.rainFlag)
        {
            bResetMoist = true;
            if (rBelt.rainSteps == 0 && ++rBelt.drySteps > 4 * cloudCover + 2)
            {
                rBelt.rainFlag = 0;
                rBelt.drySteps = 0;
            }
        }
    }
    else
    {
        rBelt.rainFlag = 1;
        rBelt.drySteps = 0;
    }
    if (rBelt.rainSteps > 0)
    {
        bResetMoist = true;
        rBelt.rainSteps = rBelt.rainSteps - 1 - cloudCover;
        if (rBelt.rainSteps < 1)
        {
            rBelt.rainSteps = 0;
            rBelt.drySteps = 0;
            if (cloudCover > 0 || alt < k_AltShoreLine)
            {
                rBelt.rainFlag = 1;
            }
        }
    }
    return bResetMoist;
}

void AdvanceBelt_(RainBelt_t& rBelt, TileRain_t& rRain, int alt, int cloudCover,
                  const MoistureDecorationConfig_t& rConfig)
{
    if (alt > k_AltShoreLine)
    {
        if (++rBelt.rainSteps > 4 * cloudCover * rConfig.rainfallCoeff + 1 && rBelt.rainFlag)
        {
            --rBelt.rainFlag;
        }
        rRain.moisture = rRain.sloping;
        return;
    }
    if (AdvanceLowlandBelt_(rBelt, alt, cloudCover))
    {
        rRain.moisture = rRain.sloping;
    }
}

// SMAC: every third column keeps a negative moisture_mod; others clear it.
void ApplyRiver_(RainBelt_t& rBelt, TileRain_t& rRain, int x)
{
    if (((x >> 1) % 3) != 0 && rRain.moistureMod < 0)
    {
        rRain.moistureMod = 0;
    }
    rBelt.rainFlag = 1;
    rBelt.drySteps = 0;
}

void AddBeltMoisture_(const RainBelt_t& rBelt, TileRain_t& rRain, int alt, int temperature,
                      int cloudCover)
{
    if (rBelt.rainFlag)
    {
        rRain.moistureMod += rBelt.rainFlag;
    }
    if (alt <= (temperature != 0) + k_AltOneAboveSea)
    {
        rRain.moisture += rRain.moistureMod;
    }
    else if (cloudCover - alt + temperature + 5 >= 2 && rRain.moistureMod > 0)
    {
        ++rRain.moisture;
    }
}

// Monsoon Jungle boosts land below the peak band (no per-tile landmark code); dunes and the
// Unity wreckage are always arid.
int LandmarkMoisture_(const Tile& rTile, int alt, int moisture)
{
    if (rTile.HasFeature("MonsoonJungle") && rTile.IsLand() && alt < k_AltThreeAboveSea)
    {
        moisture += 2;
    }
    if (rTile.HasFeature("GreatDunes") || rTile.HasFeature("UnityWreckage"))
    {
        moisture = 0;
    }
    return std::clamp(moisture, 0, 2);
}

// One tile of the row scan: its moisture tier, advancing the belt.
int RainBeltMoisture_(RainBelt_t& rBelt, const Tile& rTile, int x, int alt, int temperature,
                      int tileFlags, int cloudCover, const MoistureDecorationConfig_t& rConfig)
{
    TileRain_t rain;
    // SMAC mesa / dunes seed rain_flag at the start of the tile.
    if (rTile.HasFeature("SunnyMesa") || rTile.HasFeature("GreatDunes"))
    {
        rBelt.rainFlag = 1;
    }
    ApplyMarkFlags_(rBelt, rain, tileFlags, temperature, cloudCover);
    AdvanceBelt_(rBelt, rain, alt, cloudCover, rConfig);
    if (rTile.GetHasRiver())
    {
        ApplyRiver_(rBelt, rain, x);
    }
    AddBeltMoisture_(rBelt, rain, alt, temperature, cloudCover);
    return LandmarkMoisture_(rTile, alt, rain.moisture);
}

// Stateful rain belts → discrete Arid/Moist/Wet. Condenser bits stay in RecomputeMoisture;
// forest humidity is omitted (no world-gen forests). Pole rows are always arid.
void ScanRainBelts_(WorldMap& rWorld, RainfallGrids_t& rGrids,
                    const MoistureDecorationConfig_t& rConfig, int cloudCover)
{
    RainBelt_t belt;
    const int height = rWorld.GetHeight();
    ForEachRowTile_(rWorld, [&](Tile& rTile, int x, int y) {
        if (y <= 0 || y >= height - 1)
        {
            SetTileMoisture_(rTile, 0);
            return;
        }
        const int tier = RainBeltMoisture_(belt, rTile, x, rGrids.Alt(rWorld, rTile),
                                           rGrids.Temp(rWorld, rTile),
                                           rGrids.Flags(rWorld, rTile), cloudCover, rConfig);
        SetTileMoisture_(rTile, tier);
    });
}

bool BesideWetLand_(const WorldMap& rWorld, const Tile& rTile)
{
    return std::ranges::any_of(k_RingNeighbors, [&](const LatticeOffset_t& rOff) {
        const Tile* pNeighbor = GetTileAtLatticeOffset(rWorld, rTile, rOff.p, rOff.q);
        return pNeighbor && pNeighbor->IsLand() && pNeighbor->GetMoisture() == Moisture_t::Wet;
    });
}

// Arid land next to rainy land becomes moist.
void SmoothAridNeighbors_(WorldMap& rWorld)
{
    ForEachRowTile_(rWorld, [&](Tile& rTile, int, int) {
        if (rTile.IsLand() && rTile.GetMoisture() == Moisture_t::Arid
            && BesideWetLand_(rWorld, rTile))
        {
            SetTileMoisture_(rTile, 1);
        }
    });
}

} // namespace

void GenerateRainfall(WorldMap& rWorld,
                      const MoistureDecorationConfig_t& rConfig,
                      Rainfall_t rainfall)
{
    const int cloudCover = CloudCover_(rainfall);
    RainfallGrids_t grids = SampleTiles_(rWorld, rConfig);
    MarkPass_(rWorld, grids, rConfig, cloudCover);
    ScanRainBelts_(rWorld, grids, rConfig, cloudCover);
    SmoothAridNeighbors_(rWorld);
}

} // namespace moisture_gen
} // namespace ac

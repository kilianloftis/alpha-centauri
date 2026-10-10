#include "TestHelpers.h"
#include "game/map/MapGenerationConfig.h"
#include "game/map/MapUtils.h"
#include "game/map/MoistureGeneration.h"
#include "game/map/Tile.h"
#include "game/map/WorldGenDecorationConfig.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace ac::moisture_gen;

namespace
{

MoistureDecorationConfig_t DefaultMoisture_()
{
    return MoistureDecorationConfig_t{};
}

void FillElevation_(WorldMap& rWorld, int elevation)
{
    for (auto& pTile : rWorld.GetTiles())
    {
        pTile->SetElevation(elevation);
    }
}

} // namespace

TEST_CASE("AltitudeBand maps meters onto SMAC shore/hill/peak bands",
          "[worldgen][moisture]")
{
    const MoistureDecorationConfig_t cfg = DefaultMoisture_();
    constexpr int ocean = 0;

    CHECK(AltitudeBand(-500, ocean, cfg) == k_AltOceanShelf);
    CHECK(AltitudeBand(0, ocean, cfg) == k_AltShoreLine);
    CHECK(AltitudeBand(999, ocean, cfg) == k_AltShoreLine);
    CHECK(AltitudeBand(1000, ocean, cfg) == k_AltOneAboveSea);
    CHECK(AltitudeBand(1999, ocean, cfg) == k_AltOneAboveSea);
    CHECK(AltitudeBand(2000, ocean, cfg) == k_AltTwoAboveSea);
    CHECK(AltitudeBand(2999, ocean, cfg) == k_AltTwoAboveSea);
    CHECK(AltitudeBand(3000, ocean, cfg) == k_AltThreeAboveSea);
    CHECK(AltitudeBand(4000, ocean, cfg) == k_AltFourAboveSea);
}

TEST_CASE("West of a ridge is wetter than east of the same ridge",
          "[worldgen][moisture]")
{
    // Mid latitudes so poles do not force arid. Peak at x=20 traps cloudmass.
    WorldMap world(40, 21, actest::TestMapRules());
    FillElevation_(world, 500);
    constexpr int y = 10;
    world.GetTile(20, y)->SetElevation(2500); // hills band
    for (int x = (y & 1); x < 40; x += 2)
    {
        if (x != 20)
        {
            world.GetTile(x, y)->SetElevation(500);
        }
    }

    GenerateRainfall(world, DefaultMoisture_(), Rainfall_t::Average);

    Tile& west = *world.GetTile(16, y);
    Tile& east = *world.GetTile(24, y);
    CHECK(west.GetMoisture() != Moisture_t::Arid);
    CHECK(static_cast<int>(west.GetMoisture()) > static_cast<int>(east.GetMoisture()));
}

TEST_CASE("Peaks use cloudmass_peaks steps; hills use cloudmass_hills",
          "[worldgen][moisture]")
{
    MoistureDecorationConfig_t cfg;
    cfg.cloudmassHills = 2;
    cfg.cloudmassPeaks = 4;
    cfg.rainfallCoeff = 1;

    // Count contiguous Wet tiles west of the peak. Windward marks become Wet; neighbor
    // smooth only raises arid→Moist, so Wet reach matches cloudmass (Average rainfall).
    auto countWetWestOf = [&](int peakElev) {
        WorldMap world(40, 21, actest::TestMapRules());
        FillElevation_(world, 500);
        constexpr int y = 10;
        world.GetTile(20, y)->SetElevation(peakElev);
        GenerateRainfall(world, cfg, Rainfall_t::Average);
        int marked = 0;
        for (int step = 1; step <= 6; ++step)
        {
            Tile* pTile = world.GetTile(20 - 2 * step, y);
            if (pTile && pTile->GetMoisture() == Moisture_t::Wet)
            {
                ++marked;
            }
            else
            {
                break;
            }
        }
        return marked;
    };

    const int hillReach = countWetWestOf(2500);
    const int peakReach = countWetWestOf(3500);
    CHECK(hillReach == cfg.cloudmassHills);
    CHECK(peakReach == cfg.cloudmassPeaks);
    CHECK(peakReach > hillReach);
}

TEST_CASE("Higher planet Rainfall lengthens west belts and shortens east belts",
          "[worldgen][moisture]")
{
    MoistureDecorationConfig_t cfg;
    cfg.cloudmassHills = 3;
    cfg.cloudmassPeaks = 3;
    cfg.rainfallCoeff = 1;

    auto wetWestReach = [&](Rainfall_t rainfall) {
        WorldMap world(40, 21, actest::TestMapRules());
        FillElevation_(world, 500);
        constexpr int y = 10;
        world.GetTile(20, y)->SetElevation(2500);
        GenerateRainfall(world, cfg, rainfall);
        int marked = 0;
        for (int step = 1; step <= 6; ++step)
        {
            Tile* pTile = world.GetTile(20 - 2 * step, y);
            if (pTile && pTile->GetMoisture() == Moisture_t::Wet)
            {
                ++marked;
            }
            else
            {
                break;
            }
        }
        return marked;
    };

    // rainfall = coeff*(cloud-1): Arid → cloudmass-1, Wet → cloudmass+1
    CHECK(wetWestReach(Rainfall_t::Arid) == cfg.cloudmassHills - 1);
    CHECK(wetWestReach(Rainfall_t::Average) == cfg.cloudmassHills);
    CHECK(wetWestReach(Rainfall_t::Wet) == cfg.cloudmassHills + 1);
}

TEST_CASE("Ocean moisture advects onto land east of the shore",
          "[worldgen][moisture]")
{
    WorldMap world(24, 21, actest::TestMapRules());
    // Water on the west, flat land to the east (no peaks).
    for (auto& pTile : world.GetTiles())
    {
        pTile->SetElevation(pTile->GetX() <= 3 ? -100 : 200);
    }

    GenerateRainfall(world, DefaultMoisture_(), Rainfall_t::Average);

    constexpr int y = 10;
    Tile& eastOfOcean = *world.GetTile(4, y); // first land east of water at x=2/3
    // Shore land next to ocean should pick up ocean moisture when cloud cover >= 1.
    CHECK(eastOfOcean.IsLand());
    CHECK(eastOfOcean.GetMoisture() != Moisture_t::Arid);
}

TEST_CASE("Pole rows are forced arid", "[worldgen][moisture]")
{
    WorldMap world(20, 11, actest::TestMapRules());
    FillElevation_(world, 500);
    // Give mid-map a peak so some moisture exists inland (even parity).
    world.GetTile(10, 6)->SetElevation(2500);

    GenerateRainfall(world, DefaultMoisture_(), Rainfall_t::Wet);

    for (auto& pTile : world.GetTiles())
    {
        if (pTile->GetY() == 0 || pTile->GetY() == world.GetHeight() - 1)
        {
            CHECK(pTile->GetMoisture() == Moisture_t::Arid);
            CHECK(pTile->GetBaseMoisture() == Moisture_t::Arid);
        }
    }
}

TEST_CASE("Neighbor smooth raises arid land beside a rainy tile to moist",
          "[worldgen][moisture]")
{
    // Build a tiny world, force moisture via a strong west belt then verify smooth.
    // Peak with Wet rainfall creates Wet tiles west; a flat arid neighbor adjacent
    // diagonally/orthogonally to a Wet tile should become Moist.
    WorldMap world(30, 15, actest::TestMapRules());
    FillElevation_(world, 400);
    constexpr int y = 6;
    world.GetTile(14, y)->SetElevation(3500);

    MoistureDecorationConfig_t cfg;
    cfg.cloudmassPeaks = 5;
    cfg.rainfallCoeff = 1;
    GenerateRainfall(world, cfg, Rainfall_t::Wet);

    bool bFoundSmoothed = false;
    bool bFoundWet = false;
    for (auto& pTile : world.GetTiles())
    {
        if (pTile->IsWater())
        {
            continue;
        }
        if (pTile->GetMoisture() == Moisture_t::Wet)
        {
            bFoundWet = true;
        }
        if (pTile->GetMoisture() == Moisture_t::Moist)
        {
            // Moist can come from windward mark (moisture=1) or smooth; either is fine.
            // Require at least one Moist that has a Wet ring neighbor (smooth signature).
            for (const LatticeOffset_t& rOff : k_RingNeighbors)
            {
                Tile* pNeighbor = GetTileAtLatticeOffset(world, *pTile, rOff.p, rOff.q);
                if (pNeighbor && pNeighbor->GetMoisture() == Moisture_t::Wet)
                {
                    bFoundSmoothed = true;
                    break;
                }
            }
        }
    }
    CHECK(bFoundWet);
    CHECK(bFoundSmoothed);
}

TEST_CASE("ParseRainfall accepts case-insensitive names", "[worldgen][moisture]")
{
    CHECK(ParseRainfall("Arid") == Rainfall_t::Arid);
    CHECK(ParseRainfall("average") == Rainfall_t::Average);
    CHECK(ParseRainfall("WET") == Rainfall_t::Wet);
}

TEST_CASE("River tiles re-seed rain belts across otherwise arid flats",
          "[worldgen][moisture]")
{
    // Mid-latitude flats with no ocean moisture and no peaks stay arid unless a river
    // restores rain_flag (SMAC world_rainfall after world_rivers).
    WorldMap world(40, 21, actest::TestMapRules());
    FillElevation_(world, 400);
    constexpr int y = 10;
    for (int x = (y & 1); x < 40; x += 2)
    {
        world.GetTile(x, y)->SetHasRiver(true);
    }

    GenerateRainfall(world, DefaultMoisture_(), Rainfall_t::Average);

    int nonArid = 0;
    for (int x = (y & 1); x < 40; x += 2)
    {
        if (world.GetTile(x, y)->GetMoisture() != Moisture_t::Arid)
        {
            ++nonArid;
        }
    }
    CHECK(nonArid > 10);
}

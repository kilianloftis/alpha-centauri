#include "TestHelpers.h"
#include "game/worldgen/MapGenerationConfig.h"
#include "game/map/MapUtils.h"
#include "game/worldgen/MoistureGeneration.h"
#include "game/map/Tile.h"
#include "game/worldgen/WorldGenDecorationConfig.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

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
    WorldMap world(40, 21, actest::TestMapRules(), Rainfall_t::Average);
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

    RecomputeRainfall(world, DefaultMoisture_());

    Tile& west = *world.GetTile(16, y);
    Tile& east = *world.GetTile(24, y);
    CHECK(west.GetBaseMoisture() != Moisture_t::Arid);
    CHECK(static_cast<int>(west.GetBaseMoisture()) > static_cast<int>(east.GetBaseMoisture()));
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
        WorldMap world(40, 21, actest::TestMapRules(), Rainfall_t::Average);
        FillElevation_(world, 500);
        constexpr int y = 10;
        world.GetTile(20, y)->SetElevation(peakElev);
        RecomputeRainfall(world, cfg);
        int marked = 0;
        for (int step = 1; step <= 6; ++step)
        {
            Tile* pTile = world.GetTile(20 - 2 * step, y);
            if (pTile && pTile->GetBaseMoisture() == Moisture_t::Wet)
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
        WorldMap world(40, 21, actest::TestMapRules(), rainfall);
        FillElevation_(world, 500);
        constexpr int y = 10;
        world.GetTile(20, y)->SetElevation(2500);
        RecomputeRainfall(world, cfg);
        int marked = 0;
        for (int step = 1; step <= 6; ++step)
        {
            Tile* pTile = world.GetTile(20 - 2 * step, y);
            if (pTile && pTile->GetBaseMoisture() == Moisture_t::Wet)
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
    WorldMap world(24, 21, actest::TestMapRules(), Rainfall_t::Average);
    // Water on the west, flat land to the east (no peaks).
    for (auto& pTile : world.GetTiles())
    {
        pTile->SetElevation(pTile->GetX() <= 3 ? -100 : 200);
    }

    RecomputeRainfall(world, DefaultMoisture_());

    constexpr int y = 10;
    Tile& eastOfOcean = *world.GetTile(4, y); // first land east of water at x=2/3
    // Shore land next to ocean should pick up ocean moisture when cloud cover >= 1.
    CHECK(eastOfOcean.IsLand());
    CHECK(eastOfOcean.GetBaseMoisture() != Moisture_t::Arid);
}

TEST_CASE("Pole rows are forced arid", "[worldgen][moisture]")
{
    WorldMap world(20, 11, actest::TestMapRules(), Rainfall_t::Wet);
    FillElevation_(world, 500);
    // Give mid-map a peak so some moisture exists inland (even parity).
    world.GetTile(10, 6)->SetElevation(2500);

    RecomputeRainfall(world, DefaultMoisture_());

    for (auto& pTile : world.GetTiles())
    {
        if (pTile->GetY() == 0 || pTile->GetY() == world.GetHeight() - 1)
        {
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
    WorldMap world(30, 15, actest::TestMapRules(), Rainfall_t::Wet);
    FillElevation_(world, 400);
    constexpr int y = 6;
    world.GetTile(14, y)->SetElevation(3500);

    MoistureDecorationConfig_t cfg;
    cfg.cloudmassPeaks = 5;
    cfg.rainfallCoeff = 1;
    RecomputeRainfall(world, cfg);

    bool bFoundSmoothed = false;
    bool bFoundWet = false;
    for (auto& pTile : world.GetTiles())
    {
        if (pTile->IsWater())
        {
            continue;
        }
        if (pTile->GetBaseMoisture() == Moisture_t::Wet)
        {
            bFoundWet = true;
        }
        if (pTile->GetBaseMoisture() == Moisture_t::Moist)
        {
            // Moist can come from windward mark (moisture=1) or smooth; either is fine.
            // Require at least one Moist that has a Wet ring neighbor (smooth signature).
            for (const LatticeOffset_t& rOff : k_RingNeighbors)
            {
                Tile* pNeighbor = GetTileAtLatticeOffset(world, *pTile, rOff.p, rOff.q);
                if (pNeighbor && pNeighbor->GetBaseMoisture() == Moisture_t::Wet)
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
    WorldMap world(40, 21, actest::TestMapRules(), Rainfall_t::Average);
    FillElevation_(world, 400);
    constexpr int y = 10;
    for (int x = (y & 1); x < 40; x += 2)
    {
        world.GetTile(x, y)->SetHasRiver(true);
    }

    RecomputeRainfall(world, DefaultMoisture_());

    int nonArid = 0;
    for (int x = (y & 1); x < 40; x += 2)
    {
        if (world.GetTile(x, y)->GetBaseMoisture() != Moisture_t::Arid)
        {
            ++nonArid;
        }
    }
    CHECK(nonArid > 10);
}

namespace
{

// A fixed pseudo-random map: elevations across ocean, shore, hills and peaks, with rivers.
void FillPinnedTerrain_(WorldMap& rWorld)
{
    const MapRulesConfig_t& rRules = actest::TestMapRules();
    const int lowest = std::max(rRules.minElevationMeters, -3000);
    const int span = std::min(rRules.maxElevationMeters, 4500) - lowest;
    uint32_t state = 12345u;
    for (auto& pTile : rWorld.GetTiles())
    {
        state = state * 1664525u + 1013904223u;
        const int elevation = lowest + static_cast<int>((state >> 8) % span);
        pTile->SetElevation(elevation);
        pTile->SetHasRiver(pTile->IsLand() && (state >> 4) % 7 == 0);
    }
}

// FNV-1a over every tile's base moisture tier, in tile order.
uint64_t MoistureHash_(const WorldMap& rWorld)
{
    uint64_t hash = 1469598103934665603ull;
    for (const auto& pTile : rWorld.GetTiles())
    {
        hash ^= static_cast<uint64_t>(pTile->GetBaseMoisture());
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace

TEST_CASE("Rainfall on a fixed map reproduces its recorded moisture", "[worldgen][moisture]")
{
    // The hashes were recorded from the port as it stood; any change to them is a change to the
    // rainfall rules, to be checked against Thinker's world_rainfall.
    const std::pair<Rainfall_t, uint64_t> k_Recorded[] = {
        {Rainfall_t::Arid, 13426911896976594389ull},
        {Rainfall_t::Average, 10866242199765098256ull},
        {Rainfall_t::Wet, 1245999086220807590ull},
    };
    for (const auto& [rainfall, expected] : k_Recorded)
    {
        WorldMap world(40, 24, actest::TestMapRules(), rainfall);
        FillPinnedTerrain_(world);
        RecomputeRainfall(world, DefaultMoisture_());
        INFO("rainfall " << static_cast<int>(rainfall) << " hash " << MoistureHash_(world));
        CHECK(MoistureHash_(world) == expected);
    }
}

TEST_CASE("Rainfall writes base moisture and reports each change with its prior value",
          "[worldgen][moisture]")
{
    WorldMap world(40, 24, actest::TestMapRules(), Rainfall_t::Average);
    FillPinnedTerrain_(world);

    const std::vector<MoistureChange_t> changed = RecomputeRainfall(world, DefaultMoisture_());

    // Every tile starts arid, so the first run changes exactly the tiles it leaves non-arid.
    std::size_t nonArid = 0;
    for (const auto& pTile : world.GetTiles())
    {
        CHECK(pTile->GetMoisture() == Moisture_t::Arid);
        if (pTile->GetBaseMoisture() != Moisture_t::Arid)
        {
            ++nonArid;
        }
    }
    CHECK(nonArid > 0);
    CHECK(changed.size() == nonArid);
    CHECK(std::ranges::all_of(changed, [](const MoistureChange_t& rChange) {
        return rChange.before == Moisture_t::Arid
               && rChange.pTile->GetBaseMoisture() != Moisture_t::Arid;
    }));
    CHECK(RecomputeRainfall(world, DefaultMoisture_()).empty());
}

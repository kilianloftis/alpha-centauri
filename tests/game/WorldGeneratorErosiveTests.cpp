#include "GameFixtures.h"
#include "TestTempDir.h"

#include "game/worldgen/MapGenerationConfig.h"
#include "game/worldgen/WorldGenDecorationConfig.h"
#include "game/worldgen/WorldGenDecorationConfigParser.h"
#include "game/worldgen/WorldGenPresetConfig.h"
#include "game/worldgen/WorldGenerator.h"
#include "game/supply/SupplyPodConfig.h"
#include "game/supply/SupplyPodIndex.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <vector>

using namespace ac;

namespace
{

std::filesystem::path TempPath_(const char* name)
{
    return actest::TestTempDir::Path() / name;
}

MapGenerationConfig_t SmallMapConfig_()
{
    MapGenerationConfig_t config;
    config.width = 40;
    config.height = 30;
    config.oceanCoverage = 0.4f;
    config.seed = 12345;
    return config;
}

WorldGenDecorationConfig_t QuietDecoration_()
{
    WorldGenDecorationConfig_t decoration;
    decoration.aquifers.fraction = 0.0f;
    decoration.fungus.coverage = 0.0f;
    decoration.fungus.poleRowChance = 0.0f;
    decoration.tileBonuses.fraction = 0.0f;
    return decoration;
}

std::unique_ptr<WorldMap> GenerateWithErosive_(actest::WorldFixture& rWorld,
                                               ErosiveForces_t forces,
                                               const ErosiveDecorationConfig_t& rErosive,
                                               unsigned int seed)
{
    MapGenerationConfig_t config = SmallMapConfig_();
    config.erosiveForces = forces;
    WorldGenDecorationConfig_t decoration = QuietDecoration_();
    decoration.erosive = rErosive;
    WorldGenerator generator;
    SupplyPodIndex pods;
    return generator.Generate(config, WorldGenPresetConfig_t{}, decoration, 0, {},
                              rWorld.improvements, rWorld.dataContext.mapRules,
                              SupplyPodConfig_t{}, pods, true, seed);
}

std::vector<bool> WaterMask_(const WorldMap& rWorld)
{
    std::vector<bool> water;
    for (const auto& pTile : rWorld.GetTiles())
    {
        water.push_back(pTile->IsWater());
    }
    return water;
}

double MeanLandElevation_(const WorldMap& rWorld)
{
    double sum = 0.0;
    int count = 0;
    for (const auto& pTile : rWorld.GetTiles())
    {
        if (pTile->IsLand())
        {
            sum += static_cast<double>(pTile->GetElevation());
            ++count;
        }
    }
    REQUIRE(count > 0);
    return sum / static_cast<double>(count);
}

std::vector<int> LandElevations_(const WorldMap& rWorld)
{
    std::vector<int> elevations;
    for (const auto& pTile : rWorld.GetTiles())
    {
        if (pTile->IsLand())
        {
            elevations.push_back(pTile->GetElevation());
        }
    }
    return elevations;
}

} // namespace

TEST_CASE("WorldGenDecorationConfigParser throws when erosive object is missing",
          "[worldgen][erosive][parser]")
{
    const std::filesystem::path path = TempPath_("ac_decoration_no_erosive.json");
    {
        std::ofstream file(path);
        file << R"({
  "moisture": {
    "cloudmass_peaks": 5, "cloudmass_hills": 3, "rainfall_coeff": 1,
    "hill_min_elevation_meters": 2000, "peak_min_elevation_meters": 3000
  },
  "rockiness": {
    "poles": { "flat": 0, "rolling": 0, "rocky": 1 },
    "bands": [ { "flat": 1, "rolling": 0, "rocky": 0 } ]
  },
  "aquifers": { "fraction": 0.01 },
  "fungus": {
    "coverage": 0.3, "cluster_scale_tiles": 12, "fill": 0.85, "pole_row_chance": 0.5
  },
  "tile_bonuses": { "fraction": 0.01 }
})" << '\n';
    }

    CHECK_THROWS_WITH(WorldGenDecorationConfigParser{}.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("erosive"));
    std::filesystem::remove(path);
}

TEST_CASE("WorldGenDecorationConfigParser loads fixture erosive levels",
          "[worldgen][erosive][parser]")
{
    const WorldGenDecorationConfig_t cfg =
        WorldGenDecorationConfigParser{}.ParseConfig(
            actest::FixturePath("worldGen/decoration.json"));
    CHECK(cfg.erosive.low.landRelief == Catch::Approx(0.5f));
    CHECK(cfg.erosive.low.roughness == Catch::Approx(0.0f));
    CHECK(cfg.erosive.average.landRelief == Catch::Approx(1.0f));
    CHECK(cfg.erosive.average.roughness == Catch::Approx(0.05f));
    CHECK(cfg.erosive.high.landRelief == Catch::Approx(1.5f));
    CHECK(cfg.erosive.high.roughness == Catch::Approx(0.2f));
}

TEST_CASE("ErosiveLevelFor selects the matching decoration row", "[worldgen][erosive]")
{
    ErosiveDecorationConfig_t config;
    config.low = {0.5f, 0.0f};
    config.average = {1.0f, 0.05f};
    config.high = {1.5f, 0.2f};
    CHECK(&ErosiveLevelFor(config, ErosiveForces_t::Low) == &config.low);
    CHECK(&ErosiveLevelFor(config, ErosiveForces_t::Average) == &config.average);
    CHECK(&ErosiveLevelFor(config, ErosiveForces_t::High) == &config.high);
}

TEST_CASE("Erosive forces keep the same coastline", "[worldgen][erosive]")
{
    actest::WorldFixture world;
    ErosiveDecorationConfig_t erosive;
    erosive.low = {0.5f, 0.0f};
    erosive.high = {1.5f, 0.2f};

    const std::unique_ptr<WorldMap> pLow =
        GenerateWithErosive_(world, ErosiveForces_t::Low, erosive, 99u);
    const std::unique_ptr<WorldMap> pHigh =
        GenerateWithErosive_(world, ErosiveForces_t::High, erosive, 99u);

    CHECK(WaterMask_(*pLow) == WaterMask_(*pHigh));
}

TEST_CASE("Higher land_relief raises mean land elevation", "[worldgen][erosive]")
{
    actest::WorldFixture world;
    ErosiveDecorationConfig_t erosive;
    erosive.low = {0.5f, 0.0f};
    erosive.average = {1.0f, 0.0f};
    erosive.high = {1.5f, 0.0f};

    const std::unique_ptr<WorldMap> pLow =
        GenerateWithErosive_(world, ErosiveForces_t::Low, erosive, 77u);
    const std::unique_ptr<WorldMap> pAverage =
        GenerateWithErosive_(world, ErosiveForces_t::Average, erosive, 77u);
    const std::unique_ptr<WorldMap> pHigh =
        GenerateWithErosive_(world, ErosiveForces_t::High, erosive, 77u);

    const double lowMean = MeanLandElevation_(*pLow);
    const double averageMean = MeanLandElevation_(*pAverage);
    const double highMean = MeanLandElevation_(*pHigh);
    CHECK(lowMean < averageMean);
    CHECK(averageMean < highMean);
}

TEST_CASE("Roughness changes land elevations without moving coastlines", "[worldgen][erosive]")
{
    actest::WorldFixture world;
    ErosiveDecorationConfig_t erosive;
    erosive.low = {1.0f, 0.0f};
    erosive.high = {1.0f, 0.25f};

    const std::unique_ptr<WorldMap> pSmooth =
        GenerateWithErosive_(world, ErosiveForces_t::Low, erosive, 55u);
    const std::unique_ptr<WorldMap> pRough =
        GenerateWithErosive_(world, ErosiveForces_t::High, erosive, 55u);

    CHECK(WaterMask_(*pSmooth) == WaterMask_(*pRough));
    CHECK(LandElevations_(*pSmooth) != LandElevations_(*pRough));
}

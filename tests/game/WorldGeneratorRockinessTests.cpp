#include "GameFixtures.h"
#include "TestTempDir.h"

#include "game/worldgen/MapGenerationConfig.h"
#include "game/worldgen/RockinessGeneration.h"
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
#include <random>
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
    config.oceanCoverage = 0.3f;
    config.seed = 12345;
    return config;
}

std::vector<Rockiness_t> RockinessFlags_(const WorldMap& rWorld)
{
    std::vector<Rockiness_t> flags;
    for (const auto& pTile : rWorld.GetTiles())
    {
        flags.push_back(pTile->GetRockiness());
    }
    return flags;
}

RockinessDecorationConfig_t TwoBandConfig_()
{
    RockinessDecorationConfig_t config;
    config.poles = {0.0f, 0.0f, 1.0f};
    config.bands = {
        RockinessBand_t{0, {1.0f, 0.0f, 0.0f}},
        RockinessBand_t{std::nullopt, {0.0f, 0.0f, 1.0f}},
    };
    return config;
}

} // namespace

TEST_CASE("WorldGenDecorationConfigParser throws when rockiness object is missing",
          "[worldgen][rockiness][parser]")
{
    const std::filesystem::path path = TempPath_("ac_decoration_no_rockiness.json");
    {
        std::ofstream file(path);
        file << R"({
  "moisture": {
    "cloudmass_peaks": 5, "cloudmass_hills": 3, "rainfall_coeff": 1,
    "hill_min_elevation_meters": 2000, "peak_min_elevation_meters": 3000
  },
  "erosive": {
    "low": { "land_relief": 0.75, "roughness": 0.0 },
    "average": { "land_relief": 1.0, "roughness": 0.0 },
    "high": { "land_relief": 1.25, "roughness": 0.0 }
  },
  "aquifers": { "fraction": 0.01 },
  "fungus": {
    "coverage": 0.3, "cluster_scale_tiles": 12, "fill": 0.85, "pole_row_chance": 0.5
  },
  "tile_bonuses": { "fraction": 0.01 }
})" << '\n';
    }

    WorldGenDecorationConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("rockiness"));
    std::filesystem::remove(path);
}

TEST_CASE("WorldGenDecorationConfigParser requires poles weights", "[worldgen][rockiness][parser]")
{
    const std::filesystem::path path = TempPath_("ac_decoration_bad_poles.json");
    {
        std::ofstream file(path);
        file << R"({
  "moisture": {
    "cloudmass_peaks": 5, "cloudmass_hills": 3, "rainfall_coeff": 1,
    "hill_min_elevation_meters": 2000, "peak_min_elevation_meters": 3000
  },
  "erosive": {
    "low": { "land_relief": 0.75, "roughness": 0.0 },
    "average": { "land_relief": 1.0, "roughness": 0.0 },
    "high": { "land_relief": 1.25, "roughness": 0.0 }
  },
  "rockiness": {
    "poles": { "flat": 0, "rolling": 0, "rocky": 0 },
    "bands": [ { "flat": 1, "rolling": 0, "rocky": 0 } ]
  },
  "aquifers": { "fraction": 0.01 },
  "fungus": {
    "coverage": 0.3, "cluster_scale_tiles": 12, "fill": 0.85, "pole_row_chance": 0.5
  },
  "tile_bonuses": { "fraction": 0.01 }
})" << '\n';
    }

    WorldGenDecorationConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("rockiness.poles"));
    std::filesystem::remove(path);
}

TEST_CASE("WorldGenDecorationConfigParser requires max on non-final bands only",
          "[worldgen][rockiness][parser]")
{
    const std::filesystem::path missingMax = TempPath_("ac_decoration_band_missing_max.json");
    {
        std::ofstream file(missingMax);
        file << R"({
  "moisture": {
    "cloudmass_peaks": 5, "cloudmass_hills": 3, "rainfall_coeff": 1,
    "hill_min_elevation_meters": 2000, "peak_min_elevation_meters": 3000
  },
  "erosive": {
    "low": { "land_relief": 0.75, "roughness": 0.0 },
    "average": { "land_relief": 1.0, "roughness": 0.0 },
    "high": { "land_relief": 1.25, "roughness": 0.0 }
  },
  "rockiness": {
    "poles": { "flat": 0, "rolling": 0, "rocky": 1 },
    "bands": [
      { "flat": 1, "rolling": 0, "rocky": 0 },
      { "flat": 0, "rolling": 0, "rocky": 1 }
    ]
  },
  "aquifers": { "fraction": 0.01 },
  "fungus": {
    "coverage": 0.3, "cluster_scale_tiles": 12, "fill": 0.85, "pole_row_chance": 0.5
  },
  "tile_bonuses": { "fraction": 0.01 }
})" << '\n';
    }
    CHECK_THROWS_WITH(WorldGenDecorationConfigParser{}.ParseConfig(missingMax.string()),
                      Catch::Matchers::ContainsSubstring("non-final"));
    std::filesystem::remove(missingMax);

    const std::filesystem::path lastHasMax = TempPath_("ac_decoration_last_has_max.json");
    {
        std::ofstream file(lastHasMax);
        file << R"({
  "moisture": {
    "cloudmass_peaks": 5, "cloudmass_hills": 3, "rainfall_coeff": 1,
    "hill_min_elevation_meters": 2000, "peak_min_elevation_meters": 3000
  },
  "erosive": {
    "low": { "land_relief": 0.75, "roughness": 0.0 },
    "average": { "land_relief": 1.0, "roughness": 0.0 },
    "high": { "land_relief": 1.25, "roughness": 0.0 }
  },
  "rockiness": {
    "poles": { "flat": 0, "rolling": 0, "rocky": 1 },
    "bands": [
      { "max_elevation_above_ocean_meters": 0, "flat": 1, "rolling": 0, "rocky": 0 },
      { "max_elevation_above_ocean_meters": 1000, "flat": 0, "rolling": 0, "rocky": 1 }
    ]
  },
  "aquifers": { "fraction": 0.01 },
  "fungus": {
    "coverage": 0.3, "cluster_scale_tiles": 12, "fill": 0.85, "pole_row_chance": 0.5
  },
  "tile_bonuses": { "fraction": 0.01 }
})" << '\n';
    }
    CHECK_THROWS_WITH(WorldGenDecorationConfigParser{}.ParseConfig(lastHasMax.string()),
                      Catch::Matchers::ContainsSubstring("last entry"));
    std::filesystem::remove(lastHasMax);
}

TEST_CASE("WorldGenDecorationConfigParser requires strictly increasing band maxes",
          "[worldgen][rockiness][parser]")
{
    const std::filesystem::path path = TempPath_("ac_decoration_band_max_order.json");
    {
        std::ofstream file(path);
        file << R"({
  "moisture": {
    "cloudmass_peaks": 5, "cloudmass_hills": 3, "rainfall_coeff": 1,
    "hill_min_elevation_meters": 2000, "peak_min_elevation_meters": 3000
  },
  "erosive": {
    "low": { "land_relief": 0.75, "roughness": 0.0 },
    "average": { "land_relief": 1.0, "roughness": 0.0 },
    "high": { "land_relief": 1.25, "roughness": 0.0 }
  },
  "rockiness": {
    "poles": { "flat": 0, "rolling": 0, "rocky": 1 },
    "bands": [
      { "max_elevation_above_ocean_meters": 1000, "flat": 1, "rolling": 0, "rocky": 0 },
      { "max_elevation_above_ocean_meters": 500, "flat": 0, "rolling": 1, "rocky": 0 },
      { "flat": 0, "rolling": 0, "rocky": 1 }
    ]
  },
  "aquifers": { "fraction": 0.01 },
  "fungus": {
    "coverage": 0.3, "cluster_scale_tiles": 12, "fill": 0.85, "pole_row_chance": 0.5
  },
  "tile_bonuses": { "fraction": 0.01 }
})" << '\n';
    }

    CHECK_THROWS_WITH(WorldGenDecorationConfigParser{}.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("strictly increasing"));
    std::filesystem::remove(path);
}

TEST_CASE("WorldGenDecorationConfigParser loads shipping rockiness bands",
          "[worldgen][rockiness][parser]")
{
    const WorldGenDecorationConfig_t cfg =
        WorldGenDecorationConfigParser{}.ParseConfig(
            actest::FixturePath("worldGen/decoration.json"));
    CHECK(cfg.rockiness.poles.rocky == Catch::Approx(1.0f));
    REQUIRE(cfg.rockiness.bands.size() == 5);
    CHECK(cfg.rockiness.bands.front().maxElevationAboveOceanMeters == 0);
    CHECK_FALSE(cfg.rockiness.bands.back().maxElevationAboveOceanMeters.has_value());
}

TEST_CASE("BandForElevation picks the first matching max then the top tier",
          "[worldgen][rockiness]")
{
    const RockinessDecorationConfig_t config = TwoBandConfig_();
    CHECK(&BandForElevation(config, -100) == &config.bands[0]);
    CHECK(&BandForElevation(config, -1) == &config.bands[0]);
    CHECK(&BandForElevation(config, 0) == &config.bands[1]);
    CHECK(&BandForElevation(config, 5000) == &config.bands[1]);
}

TEST_CASE("SampleRockiness respects weight bands", "[worldgen][rockiness]")
{
    const RockinessWeights_t weights{0.5f, 0.3f, 0.2f};
    CHECK(SampleRockiness(weights, 0.0f) == Rockiness_t::Flat);
    CHECK(SampleRockiness(weights, 0.49f) == Rockiness_t::Flat);
    CHECK(SampleRockiness(weights, 0.5f) == Rockiness_t::Rolling);
    CHECK(SampleRockiness(weights, 0.79f) == Rockiness_t::Rolling);
    CHECK(SampleRockiness(weights, 0.8f) == Rockiness_t::Rocky);
    CHECK(SampleRockiness(weights, 0.999f) == Rockiness_t::Rocky);
}

TEST_CASE("SampleRockiness throws when all weights are zero", "[worldgen][rockiness]")
{
    CHECK_THROWS_WITH(SampleRockiness(RockinessWeights_t{0.0f, 0.0f, 0.0f}, 0.5f),
                      Catch::Matchers::ContainsSubstring("Invalid rockiness weights"));
}

TEST_CASE("PlaceRockiness samples poles from poles weights", "[worldgen][rockiness]")
{
    WorldMap world(20, 11, actest::TestMapRules(), Rainfall_t::Average);
    for (const auto& pTile : world.GetTiles())
    {
        pTile->SetElevation(100);
    }

    RockinessDecorationConfig_t config = TwoBandConfig_();
    config.poles = {1.0f, 0.0f, 0.0f};
    std::mt19937 rng(1);
    PlaceRockiness(world, config, rng);

    for (int x = 0; x < world.GetWidth(); x += 2)
    {
        CHECK(world.GetTile(x, 0)->GetRockiness() == Rockiness_t::Flat);
        CHECK(world.GetTile(x, world.GetHeight() - 1)->GetRockiness() == Rockiness_t::Flat);
    }
}

TEST_CASE("ParseErosiveForces accepts case-insensitive names", "[worldgen][rockiness]")
{
    CHECK(ParseErosiveForces("Low") == ErosiveForces_t::Low);
    CHECK(ParseErosiveForces("average") == ErosiveForces_t::Average);
    CHECK(ParseErosiveForces("HIGH") == ErosiveForces_t::High);
    CHECK_THROWS(ParseErosiveForces("medium"));
}

TEST_CASE("Generated rockiness is deterministic for a session seed", "[worldgen][rockiness][pipeline]")
{
    actest::WorldFixture world;
    const MapGenerationConfig_t config = SmallMapConfig_();

    WorldGenerator first;
    SupplyPodIndex firstPods;
    const std::unique_ptr<WorldMap> pFirst =
        first.Generate(config, WorldGenPresetConfig_t{}, WorldGenDecorationConfig_t{}, 0, {},
                       world.improvements, world.dataContext.mapRules, SupplyPodConfig_t{},
                       firstPods, true, 42u);
    WorldGenerator second;
    SupplyPodIndex secondPods;
    const std::unique_ptr<WorldMap> pSecond =
        second.Generate(config, WorldGenPresetConfig_t{}, WorldGenDecorationConfig_t{}, 0, {},
                        world.improvements, world.dataContext.mapRules, SupplyPodConfig_t{},
                        secondPods, true, 42u);

    CHECK(RockinessFlags_(*pFirst) == RockinessFlags_(*pSecond));
}

#include "game/map/RockinessGeneration.h"
#include "game/map/WorldGenDecorationConfigParser.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <string>

using namespace ac;
using namespace ac::rockiness_gen;
using Catch::Approx;

namespace
{

std::filesystem::path TempDecorationPath_(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

} // namespace

TEST_CASE("WorldGenDecorationConfigParser loads rockiness knobs from decoration.json",
          "[worldgen][rockiness][parser]")
{
    WorldGenDecorationConfigParser parser;
    const WorldGenDecorationConfig_t config =
        parser.ParseConfig(std::string(AC_TEST_FIXTURES_DIR) + "/../../config/worldGen/decoration.json");

    const RockinessDecorationConfig_t& r = config.rockiness;
    CHECK(r.low.flat == Approx(0.45f));
    CHECK(r.low.rolling == Approx(0.35f));
    CHECK(r.low.rocky == Approx(0.2f));
    CHECK(r.average.flat == Approx(0.55f));
    CHECK(r.average.rolling == Approx(0.35f));
    CHECK(r.average.rocky == Approx(0.1f));
    CHECK(r.high.flat == Approx(0.7f));
    CHECK(r.high.rolling == Approx(0.25f));
    CHECK(r.high.rocky == Approx(0.05f));
}

TEST_CASE("WorldGenDecorationConfigParser throws when a rockiness level is missing",
          "[worldgen][rockiness][parser]")
{
    const std::filesystem::path path = TempDecorationPath_("ac_decoration_missing_level.json");
    {
        std::ofstream file(path);
        file << R"({
  "rockiness": {
    "low": { "flat": 0.5, "rolling": 0.3, "rocky": 0.2 },
    "high": { "flat": 0.7, "rolling": 0.2, "rocky": 0.1 }
  }
})" << '\n';
    }

    WorldGenDecorationConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("average"));

    std::filesystem::remove(path);
}

TEST_CASE("WorldGenDecorationConfigParser throws when rockiness object is missing",
          "[worldgen][rockiness][parser]")
{
    const std::filesystem::path path = TempDecorationPath_("ac_decoration_no_rockiness.json");
    {
        std::ofstream file(path);
        file << R"({ "moisture": { "base_min": 0.25, "base_range": 0.5,
            "coastal_peak_bonus": 0.12, "coastal_radius": 2,
            "tropical_peak_bonus": 0.1, "tropical_half_width": 0.35,
            "orographic_strength": 0.45, "orographic_elev_scale": 1000.0,
            "orographic_max_elev": 4000.0, "arid_threshold": 0.4,
            "moist_threshold": 0.7 } })" << '\n';
    }

    WorldGenDecorationConfigParser parser;
    CHECK_THROWS_WITH(parser.ParseConfig(path.string()),
                      Catch::Matchers::ContainsSubstring("rockiness"));

    std::filesystem::remove(path);
}

TEST_CASE("WeightsForLevel returns the matching decoration table",
          "[worldgen][rockiness]")
{
    const RockinessDecorationConfig_t cfg{};
    CHECK(&WeightsForLevel(cfg, ErosiveForces_t::Low) == &cfg.low);
    CHECK(&WeightsForLevel(cfg, ErosiveForces_t::Average) == &cfg.average);
    CHECK(&WeightsForLevel(cfg, ErosiveForces_t::High) == &cfg.high);
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

TEST_CASE("SampleRockiness falls back to Flat when all weights are zero",
          "[worldgen][rockiness]")
{
    CHECK(SampleRockiness(RockinessWeights_t{0.0f, 0.0f, 0.0f}, 0.5f) == Rockiness_t::Flat);
}

TEST_CASE("ParseErosiveForces accepts case-insensitive names", "[worldgen][rockiness]")
{
    CHECK(ParseErosiveForces("Low") == ErosiveForces_t::Low);
    CHECK(ParseErosiveForces("average") == ErosiveForces_t::Average);
    CHECK(ParseErosiveForces("HIGH") == ErosiveForces_t::High);
    CHECK_THROWS(ParseErosiveForces("medium"));
}

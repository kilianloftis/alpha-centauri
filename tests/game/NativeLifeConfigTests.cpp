// config/native_life.json + native_life.lua: session keys, levels, and formulas.

#include "TempConfigFile.h"
#include "TestHelpers.h"

#include "game/natives/NativeLifeConfig.h"
#include "game/natives/PlanetFaunaCalculator.h"
#include "lib/LuaRuntime.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

using namespace ac;
using namespace actest;
using Catch::Matchers::ContainsSubstring;

namespace
{

const char* k_MinimalLua = R"LUA(
function boat_period_formula() return 1 end
function early_limit_threshold_formula() return 1 end
function boat_cargo_formula() return 1 end
function ambush_attempts_formula() return 1 end
function swarm_year_threshold_formula() return 1 end
return {
  boat_period_formula = "boat_period_formula()",
  early_limit_threshold_formula = "early_limit_threshold_formula()",
  boat_cargo_formula = "boat_cargo_formula()",
  ambush_attempts_formula = "ambush_attempts_formula()",
  swarm_year_threshold_formula = "swarm_year_threshold_formula()",
}
)LUA";

NativeLifeConfig_t Parse_(const std::string& rJson)
{
    const TempConfigFile jsonFile("native_life.json", rJson);
    const TempConfigFile luaFile("native_life.lua", k_MinimalLua);
    LuaRuntime lua;
    return NativeLifeConfigParser{}.ParseConfig(jsonFile.Path(), luaFile.Path(), lua);
}

std::string SessionKeys_()
{
    return R"(
  "fungal_bloom_native_lifeforms_min": 1,
  "fungal_bloom_native_lifeforms_max": 1,
  "planet_lifecycle_turn_thresholds": [45, 90, 170, 250, 0, 330],
  "ambush_near_base_min_dist": 2,
  "ambush_near_base_max_dist": 5,
  "boat_empty_ocean_min_region_tiles": 50,
)";
}

std::string Level_(const std::string& rId)
{
    return R"({ "id": ")" + rId + R"(", "name": ")" + rId
           + R"(", "fauna_forms": 0, "fungus_coverage_multiplier": 0, "effects": [] })";
}

} // namespace

TEST_CASE("NativeLifeConfigParser reads levels and resolves the session level",
          "[ecology][native-life][parser]")
{
    LuaRuntime lua;
    const NativeLifeConfig_t config = NativeLifeConfigParser{}.ParseConfig(
        FixturePath("native_life.json"), FixturePath("native_life.lua"), lua);
    CHECK(config.defaultId == "normal");
    CHECK(config.levels.size() == 3);
    CHECK(config.RequireForSession("").id == "normal");
    CHECK(config.RequireForSession("abundant").id == "abundant");
    CHECK_THROWS_WITH(config.RequireForSession("teeming"), ContainsSubstring("teeming"));
    CHECK(config.fungalBloomNativeLifeformsMin == 1);
    CHECK(config.ambushNearBaseMaxDist == 5);
    CHECK(config.boatEmptyOceanMinRegionTiles == 50);
    CHECK(config.RequireForSession("rare").fungusCoverageMultiplier == Catch::Approx(0.5f));
    CHECK(config.RequireForSession("normal").fungusCoverageMultiplier == Catch::Approx(1.0f));
    CHECK(config.RequireForSession("abundant").fungusCoverageMultiplier == Catch::Approx(1.5f));

    const PlanetFaunaCalculator calc(config, lua);
    CHECK(calc.BoatPeriod(1) == 8);
    CHECK(calc.AmbushAttempts(1) == 4);
}

TEST_CASE("NativeLifeConfigParser rejects bad configs", "[ecology][native-life][parser]")
{
    CHECK_NOTHROW(Parse_("{" + SessionKeys_() + R"( "default": "rare", "levels": [ )"
                         + Level_("rare") + " ] }"));
    CHECK_THROWS_WITH(Parse_("{" + SessionKeys_() + R"( "levels": [ )" + Level_("rare") + " ] }"),
                      ContainsSubstring("default"));
    CHECK_THROWS_WITH(
        Parse_("{" + SessionKeys_() + R"( "default": "rare", "levels": [] })"),
        ContainsSubstring("levels"));
    CHECK_THROWS_WITH(Parse_("{" + SessionKeys_() + R"( "default": "normal", "levels": [ )"
                             + Level_("rare") + " ] }"),
                      ContainsSubstring("normal"));
    CHECK_THROWS_WITH(Parse_("{" + SessionKeys_() + R"( "default": "rare", "levels": [ )"
                             + Level_("rare") + ", " + Level_("rare") + " ] }"),
                      ContainsSubstring("duplicate"));
    CHECK_THROWS_WITH(Parse_("{" + SessionKeys_() + R"( "default": "rare", "levels": [
        { "id": "rare", "name": "Rare", "fauna_forms": 0, "fungus_coverage_multiplier": 0,
          "effects": [], "x": 1 } ] })"),
                      ContainsSubstring("'levels[rare].x'"));
}

TEST_CASE("NativeLifeConfigParser requires every level key", "[ecology][native-life][parser]")
{
    CHECK_THROWS_WITH(Parse_("{" + SessionKeys_() + R"( "default": "rare", "levels": [
        { "name": "Rare", "fauna_forms": 0, "fungus_coverage_multiplier": 0, "effects": [] } ] })"),
                      ContainsSubstring("'levels[0].id'"));
    CHECK_THROWS_WITH(Parse_("{" + SessionKeys_() + R"( "default": "rare", "levels": [
        { "id": "rare", "fauna_forms": 0, "fungus_coverage_multiplier": 0, "effects": [] } ] })"),
                      ContainsSubstring("'levels[rare].name'"));
    CHECK_THROWS_WITH(Parse_("{" + SessionKeys_() + R"( "default": "rare", "levels": [
        { "id": "rare", "name": "Rare", "fungus_coverage_multiplier": 0, "effects": [] } ] })"),
                      ContainsSubstring("'levels[rare].fauna_forms'"));
    CHECK_THROWS_WITH(Parse_("{" + SessionKeys_() + R"( "default": "rare", "levels": [
        { "id": "rare", "name": "Rare", "fauna_forms": 0, "effects": [] } ] })"),
                      ContainsSubstring("'levels[rare].fungus_coverage_multiplier'"));
    CHECK_THROWS_WITH(Parse_("{" + SessionKeys_() + R"( "default": "rare", "levels": [
        { "id": "rare", "name": "Rare", "fauna_forms": 0, "fungus_coverage_multiplier": 0 } ] })"),
                      ContainsSubstring("'levels[rare].effects'"));
}

TEST_CASE("NativeLifeConfigParser rejects an inverted fungal lifeform range",
          "[ecology][native-life][parser]")
{
    CHECK_THROWS_WITH(Parse_(R"({
  "fungal_bloom_native_lifeforms_min": 2,
  "fungal_bloom_native_lifeforms_max": 0,
  "planet_lifecycle_turn_thresholds": [45, 90, 170, 250, 0, 330],
  "ambush_near_base_min_dist": 2,
  "ambush_near_base_max_dist": 5,
  "boat_empty_ocean_min_region_tiles": 50,
  "default": "rare",
  "levels": [ { "id": "rare", "name": "Rare", "fauna_forms": 0,
                "fungus_coverage_multiplier": 0, "effects": [] } ]
})"),
                      ContainsSubstring("fungal_bloom_native_lifeforms_max"));
}

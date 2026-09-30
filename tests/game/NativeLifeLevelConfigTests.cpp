// config/native_life_levels.json: the level list parser and the session level lookup.

#include "TempConfigFile.h"
#include "TestHelpers.h"

#include "game/NativeLifeLevelConfig.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

using namespace ac;
using namespace actest;
using Catch::Matchers::ContainsSubstring;

namespace
{

NativeLifeLevelConfig_t Parse_(const std::string& rJson)
{
    const TempConfigFile file("native_life_levels.json", rJson);
    return NativeLifeLevelConfigParser{}.ParseConfig(file.Path());
}

// A complete level entry, so each rejection case breaks exactly one thing.
std::string Level_(const std::string& rId)
{
    return R"({ "id": ")" + rId + R"(", "name": ")" + rId + R"(", "effects": [] })";
}

} // namespace

TEST_CASE("NativeLifeLevelConfigParser reads levels and resolves the session level",
          "[ecology][native-life][parser]")
{
    const NativeLifeLevelConfig_t config =
        NativeLifeLevelConfigParser{}.ParseConfig(FixturePath("native_life_levels.json"));
    CHECK(config.defaultId == "normal");
    CHECK(config.levels.size() == 3);
    CHECK(config.RequireForSession("").id == "normal");
    CHECK(config.RequireForSession("abundant").id == "abundant");
    CHECK_THROWS_WITH(config.RequireForSession("teeming"), ContainsSubstring("teeming"));
}

TEST_CASE("NativeLifeLevelConfigParser rejects bad configs", "[ecology][native-life][parser]")
{
    CHECK_NOTHROW(Parse_(R"({ "default": "rare", "levels": [ )" + Level_("rare") + " ] }"));
    CHECK_THROWS_WITH(Parse_(R"({ "levels": [ )" + Level_("rare") + " ] }"),
                      ContainsSubstring("default"));
    CHECK_THROWS_WITH(Parse_(R"({ "default": "rare", "levels": [] })"),
                      ContainsSubstring("levels"));
    CHECK_THROWS_WITH(Parse_(R"({ "default": "normal", "levels": [ )" + Level_("rare") + " ] }"),
                      ContainsSubstring("normal"));
    CHECK_THROWS_WITH(Parse_(R"({ "default": "rare", "levels": [ )" + Level_("rare") + ", "
                             + Level_("rare") + " ] }"),
                      ContainsSubstring("duplicate"));
    CHECK_THROWS_WITH(Parse_(R"({ "default": "rare", "levels": [
        { "id": "rare", "name": "Rare", "effects": [], "x": 1 } ] })"),
                      ContainsSubstring("'x'"));
}

TEST_CASE("NativeLifeLevelConfigParser requires every level key", "[ecology][native-life][parser]")
{
    CHECK_THROWS_WITH(Parse_(R"({ "default": "rare", "levels": [
        { "name": "Rare", "effects": [] } ] })"),
                      ContainsSubstring("'id'"));
    CHECK_THROWS_WITH(Parse_(R"({ "default": "rare", "levels": [
        { "id": "rare", "effects": [] } ] })"),
                      ContainsSubstring("'name'"));
    CHECK_THROWS_WITH(Parse_(R"({ "default": "rare", "levels": [
        { "id": "rare", "name": "Rare" } ] })"),
                      ContainsSubstring("'effects'"));
}

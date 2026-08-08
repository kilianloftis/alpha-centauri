#include "TestHelpers.h"

#include "game/TurnStageConfigParser.h"
#include "game/TurnStageFactory.h"
#include "game/stages/CustomTurnStage.h"
#include "game/HookContext.h"

#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace ac;

namespace
{

std::string WriteTempJson_(const std::string& contents)
{
    const std::string path =
        std::string(AC_TEST_FIXTURES_DIR) + "/turn_stage_test_config.json";
    std::ofstream out(path);
    out << contents;
    return path;
}

std::string StockTurnStagesPath_()
{
    return std::string(AC_TEST_FIXTURES_DIR) + "/../../config/turn_stages.json";
}

} // namespace

TEST_CASE("TurnStageConfigParser loads default turn_stages.json", "[TurnStageConfig]")
{
    TurnStageConfigParser parser;
    const auto stages = parser.ParseConfig(StockTurnStagesPath_());
    REQUIRE_FALSE(stages.empty());
    for (const auto& stage : stages)
    {
        CHECK(stage.id != "CustomModStage");
    }
}

TEST_CASE("TurnStageConfigParser rejects scriptPath hooks", "[TurnStageConfig]")
{
    const std::string path = WriteTempJson_(R"([
      {
        "id": "TurnStart",
        "name": "Turn Start",
        "repeatForEachFaction": false,
        "hooks": {
          "replace": [{ "modId": "m", "scriptPath": "mods/x.lua" }]
        }
      }
    ])");

    TurnStageConfigParser parser;
    CHECK_THROWS_AS(parser.ParseConfig(path), std::runtime_error);
}

TEST_CASE("TurnStageConfigParser rejects duplicate stage ids", "[TurnStageConfig]")
{
    const std::string path = WriteTempJson_(R"([
      {
        "id": "TurnStart",
        "name": "A",
        "repeatForEachFaction": false
      },
      {
        "id": "TurnStart",
        "name": "B",
        "repeatForEachFaction": false
      }
    ])");

    TurnStageConfigParser parser;
    CHECK_THROWS_AS(parser.ParseConfig(path), std::runtime_error);
}

TEST_CASE("TurnStageFactory rejects repeatForEachFaction mismatch for built-ins",
          "[TurnStageConfig]")
{
    const std::string path = WriteTempJson_(R"([
      {
        "id": "ResourceCollection",
        "name": "Resource Collection",
        "repeatForEachFaction": false
      }
    ])");

    TurnStageFactory factory;
    factory.LoadConfig(path);
    CHECK_THROWS_AS(factory.CreateStages(), std::runtime_error);
}

TEST_CASE("Custom turn stage without callable hook fails at construction",
          "[TurnStageConfig]")
{
    HookContext hooks;
    Hook_t hook;
    hook.modId = "mod";
    hooks.AddReplaceHook(hook);
    CHECK_THROWS_AS(CustomGlobalTurnStage(std::move(hooks), "Custom"), std::runtime_error);
}

TEST_CASE("Custom turn stage with callable hook constructs", "[TurnStageConfig]")
{
    HookContext hooks;
    Hook_t hook;
    hook.modId = "mod";
    hook.callback = [](const HookArgs_t&) {};
    hooks.AddReplaceHook(hook);
    CHECK_NOTHROW(CustomGlobalTurnStage(std::move(hooks), "Custom"));
}

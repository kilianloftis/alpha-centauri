#include "game/GameSettings.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <string>

using namespace ac;
using Catch::Approx;

namespace
{

std::filesystem::path TempSettingsPath(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

} // namespace

TEST_CASE("GameSettings Load leaves defaults when file is missing", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_missing.json");
    std::filesystem::remove(path);

    GameSettings settings;
    settings.Load(path.string());
    CHECK_FALSE(settings.IsPauseAtEndOfTurn());
    CHECK(settings.GetMapGeneration().width == 200);
    CHECK(settings.GetMapGeneration().height == 150);
    CHECK(settings.GetMapGeneration().oceanCoverage == Approx(0.6f));
    CHECK(settings.GetMapGeneration().presetId == "islands");
}

TEST_CASE("GameSettings Save and Load round-trip pause_at_end_of_turn", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_roundtrip.json");
    std::filesystem::remove(path);

    {
        GameSettings settings;
        settings.SetPauseAtEndOfTurn(true);
        settings.Save(path.string());
    }

    GameSettings loaded;
    loaded.Load(path.string());
    CHECK(loaded.IsPauseAtEndOfTurn());

    loaded.SetPauseAtEndOfTurn(false);
    loaded.Save(path.string());

    GameSettings reloaded;
    reloaded.Load(path.string());
    CHECK_FALSE(reloaded.IsPauseAtEndOfTurn());

    std::filesystem::remove(path);
}

TEST_CASE("GameSettings Save writes pause_at_end_of_turn JSON key", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_json_key.json");
    std::filesystem::remove(path);

    GameSettings settings;
    settings.SetPauseAtEndOfTurn(true);
    settings.Save(path.string());

    std::ifstream file(path);
    REQUIRE(file.is_open());
    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    CHECK(contents.find("pause_at_end_of_turn") != std::string::npos);
    CHECK(contents.find("true") != std::string::npos);
    CHECK(contents.find("map_generation") != std::string::npos);

    std::filesystem::remove(path);
}

TEST_CASE("GameSettings Save and Load round-trip map_generation subsection", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_map_gen.json");
    std::filesystem::remove(path);

    {
        GameSettings settings;
        MapGenerationConfig_t mapGen;
        mapGen.width = 64;
        mapGen.height = 48;
        mapGen.oceanCoverage = 0.45f;
        mapGen.presetId = "archipelago";
        mapGen.seed = 42;
        settings.SetMapGeneration(mapGen);
        settings.Save(path.string());
    }

    GameSettings loaded;
    loaded.Load(path.string());
    CHECK(loaded.GetMapGeneration().width == 64);
    CHECK(loaded.GetMapGeneration().height == 48);
    CHECK(loaded.GetMapGeneration().oceanCoverage == Approx(0.45f));
    CHECK(loaded.GetMapGeneration().presetId == "archipelago");
    CHECK(loaded.GetMapGeneration().seed == 42u);

    std::filesystem::remove(path);
}

TEST_CASE("GameSettings Load keeps map_generation defaults when subsection is absent", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_no_map_gen.json");
    std::filesystem::remove(path);

    {
        std::ofstream file(path);
        file << R"({"pause_at_end_of_turn": true})" << '\n';
    }

    GameSettings loaded;
    loaded.Load(path.string());
    CHECK(loaded.IsPauseAtEndOfTurn());
    CHECK(loaded.GetMapGeneration().width == 200);
    CHECK(loaded.GetMapGeneration().presetId == "islands");

    std::filesystem::remove(path);
}

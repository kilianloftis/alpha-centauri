#include "game/GameSettings.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
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
    CHECK_FALSE(settings.GetVisibility().removeShroud);
    CHECK_FALSE(settings.GetVisibility().removeFog);
    CHECK(settings.GetMapGeneration().width == 200);
    CHECK(settings.GetMapGeneration().height == 150);
    CHECK(settings.GetMapGeneration().oceanCoverage == Approx(0.6f));
    CHECK(settings.GetMapGeneration().erosiveForces == ErosiveForces_t::Average);
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

TEST_CASE("GameSettings Save groups keys by config struct", "[GameSettings]")
{
    // remove_shroud used to be written under game_rules and remove_fog under debug_options, so
    // the on-disk grouping did not match VisibilityConfig_t and a new knob had no obvious home.
    const std::filesystem::path path = TempSettingsPath("ac_settings_json_key.json");
    std::filesystem::remove(path);

    GameSettings settings;
    settings.SetPauseAtEndOfTurn(true);
    VisibilityConfig_t visibility;
    visibility.removeShroud = true;
    visibility.removeFog = true;
    settings.SetVisibility(visibility);
    settings.Save(path.string());

    std::ifstream file(path);
    REQUIRE(file.is_open());
    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    CHECK(contents.find("game_rules") != std::string::npos);
    CHECK(contents.find("pause_at_end_of_turn") != std::string::npos);
    CHECK(contents.find("visibility") != std::string::npos);
    CHECK(contents.find("remove_shroud") != std::string::npos);
    CHECK(contents.find("remove_fog") != std::string::npos);
    CHECK(contents.find("map_generation") != std::string::npos);
    CHECK(contents.find("graphics") != std::string::npos);
    CHECK(contents.find("debug_options") == std::string::npos);

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
        mapGen.erosiveForces = ErosiveForces_t::Low;
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
    CHECK(loaded.GetMapGeneration().erosiveForces == ErosiveForces_t::Low);
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
        file << R"({"game_rules": {"pause_at_end_of_turn": true}})" << '\n';
    }

    GameSettings loaded;
    loaded.Load(path.string());
    CHECK(loaded.IsPauseAtEndOfTurn());
    CHECK(loaded.GetMapGeneration().width == 200);
    CHECK(loaded.GetMapGeneration().presetId == "islands");

    std::filesystem::remove(path);
}

TEST_CASE("GameSettings round-trips the graphics block", "[GameSettings]")
{
    // Window size, title, FPS and font paths were compile-time literals in the SFML TU, and
    // NullGraphics carried its own copy of the size.
    const std::filesystem::path path = TempSettingsPath("ac_settings_graphics.json");
    std::filesystem::remove(path);

    {
        std::ofstream file(path);
        file << R"({"graphics": {"window_width": 640, "window_height": 480,
                    "window_title": "Modded", "framerate_limit": 30,
                    "font_paths": ["/tmp/one.ttf", "/tmp/two.ttf"]}})" << '\n';
    }

    GameSettings loaded;
    loaded.Load(path.string());
    const GraphicsConfig_t& rGraphics = loaded.GetGraphics();
    CHECK(rGraphics.windowWidth == 640);
    CHECK(rGraphics.windowHeight == 480);
    CHECK(rGraphics.windowTitle == "Modded");
    CHECK(rGraphics.framerateLimit == 30);
    REQUIRE(rGraphics.fontPaths.size() == 2);
    CHECK(rGraphics.fontPaths[0] == "/tmp/one.ttf");

    std::filesystem::remove(path);
}

TEST_CASE("GameSettings rejects an unusable graphics block", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_bad_graphics.json");

    SECTION("zero window size")
    {
        {
            std::ofstream file(path);
            file << R"({"graphics": {"window_width": 0}})" << '\n';
        }
        GameSettings loaded;
        CHECK_THROWS_WITH(loaded.Load(path.string()),
                          Catch::Matchers::ContainsSubstring("window_width"));
    }

    SECTION("no fonts named")
    {
        {
            std::ofstream file(path);
            file << R"({"graphics": {"font_paths": []}})" << '\n';
        }
        GameSettings loaded;
        CHECK_THROWS_WITH(loaded.Load(path.string()),
                          Catch::Matchers::ContainsSubstring("font_paths"));
    }

    std::filesystem::remove(path);
}

TEST_CASE("GameSettings rejects an unusable map_generation block", "[GameSettings]")
{
    // Load is the trust boundary for a hand-editable file. These used to be taken verbatim and
    // surface as a failure from inside world generation, if at all.
    const std::filesystem::path path = TempSettingsPath("ac_settings_bad_map_gen.json");

    const auto writeMapGen = [&path](const std::string& rBody) {
        std::ofstream file(path);
        file << R"({"map_generation": )" << rBody << "}" << '\n';
    };

    SECTION("non-positive dimensions")
    {
        writeMapGen(R"({"width": 0, "height": 40})");
        GameSettings loaded;
        CHECK_THROWS_WITH(loaded.Load(path.string()),
                          Catch::Matchers::ContainsSubstring("width"));
    }

    SECTION("ocean coverage outside [0, 1]")
    {
        writeMapGen(R"({"ocean_coverage": 1.5})");
        GameSettings loaded;
        CHECK_THROWS_WITH(loaded.Load(path.string()),
                          Catch::Matchers::ContainsSubstring("ocean_coverage"));
    }

    SECTION("empty preset id")
    {
        writeMapGen(R"({"preset_id": ""})");
        GameSettings loaded;
        CHECK_THROWS_WITH(loaded.Load(path.string()),
                          Catch::Matchers::ContainsSubstring("preset_id"));
    }

    std::filesystem::remove(path);
}

TEST_CASE("GameSettings Save and Load round-trip remove_shroud and remove_fog", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_visibility.json");
    std::filesystem::remove(path);

    {
        GameSettings settings;
        VisibilityConfig_t visibility;
        visibility.removeShroud = true;
        visibility.removeFog = true;
        settings.SetVisibility(visibility);
        settings.Save(path.string());
    }

    GameSettings loaded;
    loaded.Load(path.string());
    CHECK(loaded.GetVisibility().removeShroud);
    CHECK(loaded.GetVisibility().removeFog);

    std::filesystem::remove(path);
}

TEST_CASE("GameSettings SetVisibility emits OnVisibilityChanged only on change", "[GameSettings]")
{
    GameSettings settings;
    int emissions = 0;
    auto connection = settings.OnVisibilityChanged.ConnectScoped([&]() { ++emissions; });

    VisibilityConfig_t visibility;
    visibility.removeFog = true;
    settings.SetVisibility(visibility);
    CHECK(emissions == 1);

    settings.SetVisibility(visibility);
    CHECK(emissions == 1);

    visibility.removeFog = false;
    settings.SetVisibility(visibility);
    CHECK(emissions == 2);
}

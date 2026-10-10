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
    CHECK(settings.IsAutoReturnLowFuelAir());
    CHECK_FALSE(settings.GetVisibility().removeShroud);
    CHECK_FALSE(settings.GetVisibility().removeFog);
    CHECK(settings.GetPauseOnEvents().newFacilityBuilt);
    CHECK(settings.GetPauseOnEvents().buildOrdersOutOfDate);
    CHECK(settings.GetMapGeneration().width == 200);
    CHECK(settings.GetMapGeneration().height == 300);
    CHECK(settings.GetMapGeneration().oceanCoverage == Approx(0.6f));
    CHECK(settings.GetMapGeneration().erosiveForces == ErosiveForces_t::Average);
    CHECK(settings.GetMapGeneration().rainfall == Rainfall_t::Average);
    CHECK(settings.GetMapGeneration().presetId == "islands");
    // Empty, not a hard-coded level: difficulty.json's "default" owns the shipping choice.
    CHECK(settings.GetGameRules().difficultyId.empty());
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

TEST_CASE("GameSettings Save and Load round-trip difficulty", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_difficulty.json");
    std::filesystem::remove(path);

    {
        GameSettings settings;
        GameRulesConfig_t rules = settings.GetGameRules();
        rules.difficultyId = "transcend";
        settings.SetGameRules(rules);
        settings.Save(path.string());
    }

    GameSettings loaded;
    loaded.Load(path.string());
    CHECK(loaded.GetGameRules().difficultyId == "transcend");

    std::filesystem::remove(path);
}

TEST_CASE("GameSettings Save and Load round-trip native life level", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_native_life.json");
    std::filesystem::remove(path);

    {
        GameSettings settings;
        CHECK(settings.GetGameRules().nativeLifeLevelId.empty());
        GameRulesConfig_t rules = settings.GetGameRules();
        rules.nativeLifeLevelId = "abundant";
        settings.SetGameRules(rules);
        settings.Save(path.string());
    }

    GameSettings loaded;
    loaded.Load(path.string());
    CHECK(loaded.GetGameRules().nativeLifeLevelId == "abundant");

    std::filesystem::remove(path);
}

TEST_CASE("GameSettings Save and Load round-trip auto_return_low_fuel_air", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_auto_return.json");
    std::filesystem::remove(path);

    {
        GameSettings settings;
        CHECK(settings.IsAutoReturnLowFuelAir());
        settings.SetAutoReturnLowFuelAir(false);
        settings.Save(path.string());
    }

    GameSettings loaded;
    loaded.Load(path.string());
    CHECK_FALSE(loaded.IsAutoReturnLowFuelAir());

    loaded.SetAutoReturnLowFuelAir(true);
    loaded.Save(path.string());

    GameSettings reloaded;
    reloaded.Load(path.string());
    CHECK(reloaded.IsAutoReturnLowFuelAir());

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
    CHECK(contents.find("auto_return_low_fuel_air") != std::string::npos);
    CHECK(contents.find("visibility") != std::string::npos);
    CHECK(contents.find("remove_shroud") != std::string::npos);
    CHECK(contents.find("remove_fog") != std::string::npos);
    CHECK(contents.find("pause_on_events") != std::string::npos);
    CHECK(contents.find("new_facility_built") != std::string::npos);
    CHECK(contents.find("build_orders_out_of_date") != std::string::npos);
    CHECK(contents.find("map_generation") != std::string::npos);
    CHECK(contents.find("map_display") != std::string::npos);
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
        mapGen.rainfall = Rainfall_t::Wet;
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
    CHECK(loaded.GetMapGeneration().rainfall == Rainfall_t::Wet);
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

TEST_CASE("GameSettings loads SMAC font path before system fallbacks", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_smac_font.json");
    std::filesystem::remove(path);

    {
        std::ofstream file(path);
        file << R"({"graphics": {"font_paths": [
                    "assets/ui/fonts/arialn.ttf",
                    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"]}})"
             << '\n';
    }

    GameSettings loaded;
    loaded.Load(path.string());
    const GraphicsConfig_t& rGraphics = loaded.GetGraphics();
    REQUIRE(rGraphics.fontPaths.size() == 3);
    CHECK(rGraphics.fontPaths[0] == "assets/ui/fonts/arialn.ttf");
    CHECK(rGraphics.fontPaths[1] == "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");

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

    SECTION("odd width")
    {
        writeMapGen(R"({"width": 201, "height": 40})");
        GameSettings loaded;
        CHECK_THROWS_WITH(loaded.Load(path.string()),
                          Catch::Matchers::ContainsSubstring("width must be even"));
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

TEST_CASE("GameSettings Save and Load round-trip pause_on_events", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_pause_on_events.json");
    std::filesystem::remove(path);

    {
        GameSettings settings;
        PauseOnEventsConfig_t config = settings.GetPauseOnEvents();
        config.newFacilityBuilt = false;
        config.droneRiots = false;
        config.buildOrdersOutOfDate = false;
        config.atrocityCommitted = false;
        config.fungalBloom = false;
        settings.SetPauseOnEvents(config);
        settings.Save(path.string());
    }

    GameSettings loaded;
    loaded.Load(path.string());
    CHECK_FALSE(loaded.GetPauseOnEvents().newFacilityBuilt);
    CHECK(loaded.GetPauseOnEvents().combatUnitBuilt);
    CHECK_FALSE(loaded.GetPauseOnEvents().droneRiots);
    CHECK_FALSE(loaded.GetPauseOnEvents().buildOrdersOutOfDate);
    CHECK_FALSE(loaded.GetPauseOnEvents().atrocityCommitted);
    CHECK_FALSE(loaded.GetPauseOnEvents().fungalBloom);

    std::filesystem::remove(path);
}

TEST_CASE("GameSettings SetPauseOnEvents emits only on change", "[GameSettings]")
{
    GameSettings settings;
    int emissions = 0;
    auto connection = settings.OnPauseOnEventsChanged.ConnectScoped([&]() { ++emissions; });

    PauseOnEventsConfig_t config = settings.GetPauseOnEvents();
    config.newFacilityBuilt = false;
    settings.SetPauseOnEvents(config);
    CHECK(emissions == 1);

    settings.SetPauseOnEvents(config);
    CHECK(emissions == 1);

    config.newFacilityBuilt = true;
    settings.SetPauseOnEvents(config);
    CHECK(emissions == 2);
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

TEST_CASE("GameSettings Save and Load round-trip map_display", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_map_display.json");
    std::filesystem::remove(path);

    {
        GameSettings settings;
        settings.SetMapDisplay(MapDisplayConfig_t{ReliefMode_t::Stepped, /*bOceanGrid*/ true});
        settings.Save(path.string());
    }

    GameSettings loaded;
    loaded.Load(path.string());
    CHECK(loaded.GetMapDisplay().relief == ReliefMode_t::Stepped);
    CHECK(loaded.GetMapDisplay().bOceanGrid);

    std::filesystem::remove(path);
}

TEST_CASE("GameSettings defaults to smooth relief without an ocean grid", "[GameSettings]")
{
    const GameSettings settings;
    CHECK(settings.GetMapDisplay().relief == ReliefMode_t::Smooth);
    CHECK_FALSE(settings.GetMapDisplay().bOceanGrid);
}

TEST_CASE("GameSettings rejects an unknown relief mode", "[GameSettings]")
{
    const std::filesystem::path path = TempSettingsPath("ac_settings_bad_relief.json");
    {
        std::ofstream file(path);
        file << R"({"map_display": {"relief": "bumpy"}})";
    }

    GameSettings settings;
    CHECK_THROWS_WITH(settings.Load(path.string()),
                      Catch::Matchers::ContainsSubstring("map_display.relief"));

    std::filesystem::remove(path);
}

TEST_CASE("GameSettings SetMapDisplay emits OnMapDisplayChanged only on change", "[GameSettings]")
{
    GameSettings settings;
    int emissions = 0;
    auto connection = settings.OnMapDisplayChanged.ConnectScoped([&]() { ++emissions; });

    MapDisplayConfig_t display;
    display.relief = ReliefMode_t::Flat;
    settings.SetMapDisplay(display);
    CHECK(emissions == 1);

    settings.SetMapDisplay(display);
    CHECK(emissions == 1);
}

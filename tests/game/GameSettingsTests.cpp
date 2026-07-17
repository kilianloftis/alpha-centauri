#include "game/GameSettings.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <string>

using namespace ac;

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

    std::filesystem::remove(path);
}

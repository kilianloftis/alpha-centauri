#include "RecordingGraphics.h"
#include "TestHelpers.h"

#include "game/GameSettings.h"
#include "ui/settings/SettingsPanel.h"
#include "ui/style/UiStyle.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

using namespace ac;
using actest::RecordingGraphics;

namespace
{

void EnsureStyleLoaded_()
{
    static const bool bLoaded = []
    {
        UiStyle::Load(actest::FixturePath("ui/style.json"));
        return true;
    }();
    (void)bLoaded;
}

// The text of the row starting with prefix, and where the panel drew it.
std::optional<RecordingGraphics::TextDraw_t> Row_(SettingsPanel& rPanel, const std::string& prefix)
{
    RecordingGraphics graphics;
    rPanel.Render(graphics);
    for (const RecordingGraphics::TextDraw_t& rText : graphics.texts)
    {
        if (rText.text.starts_with(prefix))
        {
            return rText;
        }
    }
    return std::nullopt;
}

void Click_(SettingsPanel& rPanel, const RecordingGraphics::TextDraw_t& rRow)
{
    rPanel.HandleMouseClick(MouseEvent_t{MouseButton_t::Left, static_cast<int>(rRow.x) + 2,
                                         static_cast<int>(rRow.y) + 2, {}});
}

std::string FileText_(const std::filesystem::path& rPath)
{
    std::ifstream file(rPath);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("The elevation row cycles smooth, stepped and flat, saving each change",
          "[ui][settings]")
{
    EnsureStyleLoaded_();
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_settings_panel_relief.json";
    std::filesystem::remove(path);
    GameSettings settings;
    settings.SetSavePath(path.string());
    SettingsPanel panel(settings, WindowLayout_t{0.0f, 0.0f, 800.0f, 1600.0f});

    const auto row = Row_(panel, "Elevation");
    REQUIRE(row);
    CHECK(row->text == "Elevation: Smooth");

    Click_(panel, *row);
    CHECK(settings.GetMapDisplay().relief == ReliefMode_t::Stepped);
    CHECK(Row_(panel, "Elevation")->text == "Elevation: Stepped");
    CHECK(FileText_(path).find("\"stepped\"") != std::string::npos);

    Click_(panel, *row);
    CHECK(settings.GetMapDisplay().relief == ReliefMode_t::Flat);
    Click_(panel, *row);
    CHECK(settings.GetMapDisplay().relief == ReliefMode_t::Smooth);

    std::filesystem::remove(path);
}

TEST_CASE("The ocean grid row toggles the ocean grid", "[ui][settings]")
{
    EnsureStyleLoaded_();
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_settings_panel_ocean_grid.json";
    std::filesystem::remove(path);
    GameSettings settings;
    settings.SetSavePath(path.string());
    SettingsPanel panel(settings, WindowLayout_t{0.0f, 0.0f, 800.0f, 1600.0f});

    const auto row = Row_(panel, "Ocean Grid");
    REQUIRE(row);
    CHECK(row->text == "Ocean Grid: Off");
    Click_(panel, *row);
    CHECK(settings.GetMapDisplay().bOceanGrid);

    std::filesystem::remove(path);
}

// The satellite screen's two per-frame findings: the summary grid rebuilt on every paint, and
// selection handled by tearing down and recreating the whole view. Package 15 commit B.

#include "ViewFixture.h"

#include "game/GameState.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "ui/satellite/SatelliteButtonListPanel.h"
#include "ui/satellite/SatelliteLabeledButton.h"
#include "ui/satellite/SatelliteSummaryPanel.h"
#include "ui/style/UiStyle.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using actest::RecordingGraphics;
using actest::ViewFixture;

namespace
{

// An orbital building in the fixture registry, so the census has something to count.
constexpr const char* k_OrbitalId = "Sky_Hydroponics_Lab";

} // namespace

TEST_CASE("The summary grid is cached, and Refresh is what updates it", "[ui][satellite]")
{
    // Render rebuilt the orbital-type vector, the faction list and a string-keyed census map on
    // every paint, and BuildOrbitalCensus walks every faction's bases and buildings.
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);

    SatelliteSummaryPanel panel(*fixture.pState, *fixture.dataContext.buildingRegistry,
                                ViewFixture::FullScreen());
    panel.Render(fixture.graphics);
    // One faction, one row: the cell for the orbital type reads exactly "0".
    REQUIRE_FALSE(fixture.graphics.TextYs("0").empty());
    REQUIRE(fixture.graphics.TextYs("1").empty());

    rBase.GetBuildingManager().AddBuilding(k_OrbitalId);

    // Painting alone does not re-census — that is the whole point of the cache.
    fixture.graphics.texts.clear();
    panel.Render(fixture.graphics);
    CHECK(fixture.graphics.TextYs("1").empty());

    panel.Refresh();
    fixture.graphics.texts.clear();
    panel.Render(fixture.graphics);
    CHECK_FALSE(fixture.graphics.TextYs("1").empty());
}

TEST_CASE("A list panel applies selection to its own buttons", "[ui][satellite]")
{
    // SatelliteButtonListPanel claimed mutually-exclusive selection but never updated
    // m_selectedId or any button; it only ever looked right because the parent view destroyed
    // and rebuilt the entire panel on each click.
    ViewFixture::EnsureStyleLoaded();

    const WindowLayout_t layout{0.0f, 0.0f, 300.0f, 400.0f};
    SatelliteButtonListPanel panel(
        layout, "Enemy Factions",
        {SatelliteButtonListPanel::Item_t{"1", "Alpha"},
         SatelliteButtonListPanel::Item_t{"2", "Beta"}},
        std::nullopt,
        [](const std::string&) {});

    const auto& style = Style().satelliteView;
    const float firstX = layout.x + style.listPadX;
    const float firstY = layout.y + style.listPadY + style.listHeaderHeight + style.listButtonGap;
    const float secondY = firstY + style.listButtonHeight + style.listButtonGap;

    RecordingGraphics unselected;
    panel.Render(unselected);
    const auto firstUnselected = unselected.FillColorAt(firstX, firstY);
    REQUIRE(firstUnselected.has_value());

    panel.SetSelected(std::string("1"));
    RecordingGraphics selected;
    panel.Render(selected);
    const auto firstSelected = selected.FillColorAt(firstX, firstY);
    const auto secondSelected = selected.FillColorAt(firstX, secondY);
    REQUIRE(firstSelected.has_value());
    REQUIRE(secondSelected.has_value());

    // The selected row's fill changed; the other row's did not.
    CHECK(firstSelected->r == style.tabSelectedFillColor.r);
    CHECK(firstSelected->g == style.tabSelectedFillColor.g);
    CHECK(firstSelected->b == style.tabSelectedFillColor.b);
    CHECK(secondSelected->r == style.tabFillColor.r);
    CHECK(secondSelected->g == style.tabFillColor.g);
    CHECK(secondSelected->b == style.tabFillColor.b);

    // Selection is exclusive: moving it clears the previous row.
    panel.SetSelected(std::string("2"));
    RecordingGraphics moved;
    panel.Render(moved);
    CHECK(moved.FillColorAt(firstX, firstY)->r == style.tabFillColor.r);
    CHECK(moved.FillColorAt(firstX, secondY)->r == style.tabSelectedFillColor.r);
}

TEST_CASE("Replacing a list's items keeps the requested selection", "[ui][satellite]")
{
    // The target list is rebuilt from scratch when the selected faction changes; the selection
    // it is handed must survive that.
    ViewFixture::EnsureStyleLoaded();

    const WindowLayout_t layout{0.0f, 0.0f, 300.0f, 400.0f};
    SatelliteButtonListPanel panel(layout, "Orbital Targets", {}, std::nullopt,
                                   [](const std::string&) {});

    panel.SetItems({SatelliteButtonListPanel::Item_t{k_OrbitalId, "Sky Hydroponics Lab (2)"}},
                   std::string(k_OrbitalId));

    RecordingGraphics graphics;
    panel.Render(graphics);

    const auto& style = Style().satelliteView;
    const float x = layout.x + style.listPadX;
    const float y = layout.y + style.listPadY + style.listHeaderHeight + style.listButtonGap;
    CHECK(graphics.AnyTextContaining("Sky Hydroponics Lab (2)"));
    REQUIRE(graphics.FillColorAt(x, y).has_value());
    CHECK(graphics.FillColorAt(x, y)->r == style.tabSelectedFillColor.r);
}

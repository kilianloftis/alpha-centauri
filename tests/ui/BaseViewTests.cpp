// BaseView's display snapshot: it must not recompute on every paint, and it must not go stale
// when the player acts. Package 15 commit B.

#include "ViewFixture.h"

#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "game/population/pop-types/Pop.h"
#include "ui/base/BaseDisplaySnapshot.h"
#include "ui/base/BuildingsDisplay.h"
#include "ui/style/UiStyle.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using actest::RecordingGraphics;
using actest::ViewFixture;

namespace
{

bool SameColor_(const Color_t& rLeft, const Color_t& rRight)
{
    return rLeft.r == rRight.r && rLeft.g == rRight.g && rLeft.b == rRight.b
        && rLeft.a == rRight.a;
}

const RecordingGraphics::TextDraw_t* DrawnText_(const RecordingGraphics& rGraphics,
                                                const std::string& rExact)
{
    for (const RecordingGraphics::TextDraw_t& rDraw : rGraphics.texts)
    {
        if (rDraw.text == rExact)
        {
            return &rDraw;
        }
    }
    return nullptr;
}

// The fixture's pop types configure no fallback, so UserUnassignTile (which converts the freed
// worker) is not available here; releasing the claim directly is the same key input.
Pop* FindWorkingPop(BaseManager& rBase)
{
    for (Pop& rPop : rBase.GetPopulation().Pops())
    {
        if (rPop.IsWorker() && rPop.GetTile())
        {
            return &rPop;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("The base display key is stable across paints and moves when the player acts",
          "[ui][base]")
{
    // The key is what BaseView consults every frame instead of rebuilding: if it moved on its
    // own the snapshot would be rebuilt per frame anyway, and if it failed to move on an
    // assignment the panels would show yesterday's yields.
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);

    const BaseDisplayKey_t initial = ReadBaseDisplayKey(rBase);
    CHECK(ReadBaseDisplayKey(rBase) == initial);
    CHECK(ReadBaseDisplayKey(rBase) == initial);

    // Free a worked tile: the worked-tile index revision must move.
    Pop* pWorking = FindWorkingPop(rBase);
    REQUIRE(pWorking != nullptr);

    rBase.GetWorkerAssignments().UnassignWorker(*pWorking);
    CHECK_FALSE(ReadBaseDisplayKey(rBase) == initial);
}

TEST_CASE("The snapshot describes every workable tile the panel draws", "[ui][base]")
{
    // BaseWorkableAreaDisplay throws on a tile the snapshot does not carry; both walk
    // GetWorkableTiles, and this pins that they cannot drift apart.
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);

    const BaseDisplaySnapshot_t snapshot = BuildBaseDisplaySnapshot(rBase);
    const auto& rWorkable = rBase.GetWorkerAssignments().GetWorkableTiles();
    REQUIRE_FALSE(rWorkable.empty());
    CHECK(snapshot.tiles.size() == rWorkable.size());
    for (const Tile* pTile : rWorkable)
    {
        CHECK(snapshot.tiles.count(pTile) == 1);
    }
}

TEST_CASE("The snapshot reports the same yields the panel used to query live", "[ui][base]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);

    const BaseDisplaySnapshot_t snapshot = BuildBaseDisplaySnapshot(rBase);
    CHECK(snapshot.nutrientProduction == rBase.GetNutrientProduction());
    CHECK(snapshot.nutrientsRequired == rBase.GetNutrientsRequired());
    CHECK(snapshot.mineralProduction == rBase.GetMineralProduction());

    const WorkerAssignmentManager& rAssignments = rBase.GetWorkerAssignments();
    for (const auto& [pTile, rEntry] : snapshot.tiles)
    {
        // Assert the work state itself, not just the yield picked using it — otherwise a wrong
        // work state selects a wrong-but-matching live value and the check passes.
        const TileWorkState_t expected =
            rAssignments.IsTileWorkedByThisBase(pTile)  ? TileWorkState_t::WorkedByThisBase
            : rAssignments.IsTileAssigned(pTile)        ? TileWorkState_t::WorkedByOther
                                                        : TileWorkState_t::Free;
        CHECK(rEntry.workState == expected);

        const TileYieldView_t live = expected == TileWorkState_t::WorkedByThisBase
                                         ? rBase.GetWorkedTileYield(*pTile)
                                         : rBase.GetPreviewTileYield(*pTile);
        CHECK(rEntry.yield.effective.nutrients == live.effective.nutrients);
        CHECK(rEntry.yield.effective.minerals == live.effective.minerals);
        CHECK(rEntry.yield.effective.energy == live.effective.energy);
    }
}

TEST_CASE("BaseView repaints without rebuilding the snapshot, and refreshes after a change",
          "[ui][base]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);

    pView->Render(fixture.graphics);
    const size_t drawsPerFrame = fixture.graphics.texts.size();
    REQUIRE(drawsPerFrame > 0);
    // Built once, at construction.
    REQUIRE(pView->GetSnapshotBuildCount() == 1);

    // Repainting an unchanged base draws the same thing and does no work.
    for (int i = 0; i < 10; ++i)
    {
        fixture.graphics.texts.clear();
        pView->Render(fixture.graphics);
        CHECK(fixture.graphics.texts.size() == drawsPerFrame);
    }
    CHECK(pView->GetSnapshotBuildCount() == 1);

    // Freeing a worked tile changes the nutrient total; the panel must not keep painting the
    // pre-click snapshot.
    Pop* pWorking = FindWorkingPop(rBase);
    REQUIRE(pWorking != nullptr);
    const int beforeNutrients = rBase.GetNutrientProduction();

    rBase.GetWorkerAssignments().UnassignWorker(*pWorking);
    const int afterNutrients = rBase.GetNutrientProduction();
    REQUIRE(beforeNutrients != afterNutrients);

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(pView->GetSnapshotBuildCount() == 2);
    CHECK(fixture.graphics.AnyTextContaining("Production: " + std::to_string(afterNutrients)));
    CHECK_FALSE(fixture.graphics.AnyTextContaining("Production: " + std::to_string(beforeNutrients)));
}

TEST_CASE("BaseView lists every constructed building by name, stacked vertically", "[ui][base]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    BuildingManager& rBuildings = rBase.GetBuildingManager();

    rBuildings.AddBuilding("test_facility_a");
    rBuildings.AddBuilding("test_facility_b");
    rBuildings.AddBuilding("test_facility_a");

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);

    const auto facilityA = fixture.graphics.TextYs("Test Facility A");
    const auto facilityB = fixture.graphics.TextYs("Test Facility B");
    REQUIRE(facilityA.size() == 2);
    REQUIRE(facilityB.size() == 1);
    CHECK(facilityA[0] < facilityB[0]);
    CHECK(facilityB[0] < facilityA[1]);
}

TEST_CASE("The buildings list is live: a paint after add or destroy shows the new set",
          "[ui][base]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);

    const WindowLayout_t layout{0.0f, 0.0f, 200.0f, 400.0f};
    BuildingsDisplay display(rBase, layout);

    display.Render(fixture.graphics);
    CHECK_FALSE(fixture.graphics.AnyTextContaining("Test Facility A"));
    CHECK_FALSE(fixture.graphics.AnyTextContaining("Test Facility B"));

    rBase.GetBuildingManager().AddBuilding("test_facility_a");
    rBase.GetBuildingManager().AddBuilding("test_facility_b");
    fixture.graphics.texts.clear();
    display.Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("Test Facility A"));
    CHECK(fixture.graphics.AnyTextContaining("Test Facility B"));

    rBase.GetBuildingManager().DestroyBuilding("test_facility_a");
    fixture.graphics.texts.clear();
    display.Render(fixture.graphics);
    CHECK_FALSE(fixture.graphics.AnyTextContaining("Test Facility A"));
    CHECK(fixture.graphics.AnyTextContaining("Test Facility B"));
}

TEST_CASE("A granted-only building is listed in the darker granted colour", "[ui][base]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    rBase.GetBuildingManager().AddBuilding("grantor_local");

    BuildingsDisplay display(rBase, WindowLayout_t{0.0f, 0.0f, 200.0f, 400.0f});
    display.Render(fixture.graphics);

    const RecordingGraphics::TextDraw_t* pGranted = DrawnText_(fixture.graphics, "Granted Hall");
    const RecordingGraphics::TextDraw_t* pGrantor =
        DrawnText_(fixture.graphics, "Grantor (ThisBase scope)");
    REQUIRE(pGranted != nullptr);
    REQUIRE(pGrantor != nullptr);

    const auto& style = Style().buildingsDisplay;
    CHECK(SameColor_(pGranted->color, style.grantedTextColor));
    CHECK(SameColor_(pGrantor->color, style.textColor));
    CHECK_FALSE(fixture.graphics.AnyTextContaining("* Granted Hall"));
}

TEST_CASE("A constructed building that is also granted is listed with a * in the granted colour",
          "[ui][base]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    rBase.GetBuildingManager().AddBuilding("granted_hall");
    rBase.GetBuildingManager().AddBuilding("grantor_local");

    BuildingsDisplay display(rBase, WindowLayout_t{0.0f, 0.0f, 200.0f, 400.0f});
    display.Render(fixture.graphics);

    const RecordingGraphics::TextDraw_t* pBoth = DrawnText_(fixture.graphics, "* Granted Hall");
    REQUIRE(pBoth != nullptr);
    CHECK(SameColor_(pBoth->color, Style().buildingsDisplay.grantedTextColor));
    CHECK(fixture.graphics.TextYs("Granted Hall").empty());
}

TEST_CASE("A ThisBase grant does not appear on another base's buildings list", "[ui][base]")
{
    ViewFixture fixture;
    BaseManager& rGrantorBase = fixture.MakeBase(4, 4);
    BaseManager& rOtherBase = fixture.MakeBase(6, 6);
    rGrantorBase.GetBuildingManager().AddBuilding("grantor_local");

    BuildingsDisplay display(rOtherBase, WindowLayout_t{0.0f, 0.0f, 200.0f, 400.0f});
    display.Render(fixture.graphics);

    CHECK_FALSE(fixture.graphics.AnyTextContaining("Granted Hall"));
}

TEST_CASE("A faction-global grant appears on every base in the granted colour", "[ui][base]")
{
    ViewFixture fixture;
    BaseManager& rGrantorBase = fixture.MakeBase(4, 4);
    BaseManager& rOtherBase = fixture.MakeBase(6, 6);
    rGrantorBase.GetBuildingManager().AddBuilding("grantor_global");

    BuildingsDisplay display(rOtherBase, WindowLayout_t{0.0f, 0.0f, 200.0f, 400.0f});
    display.Render(fixture.graphics);

    const RecordingGraphics::TextDraw_t* pGranted = DrawnText_(fixture.graphics, "Granted Hall");
    REQUIRE(pGranted != nullptr);
    CHECK(SameColor_(pGranted->color, Style().buildingsDisplay.grantedTextColor));
}

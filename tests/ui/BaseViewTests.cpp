// BaseView's display snapshot: it must not recompute on every paint, and it must not go stale
// when the player acts. Package 15 commit B.

#include "ViewFixture.h"

#include "game/buildings/BuildingConfig.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/production/HurryProductionCalculator.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/EconomyManager.h"
#include "game/Faction.h"
#include "game/map/Tile.h"
#include "game/population/pop-types/Pop.h"
#include "ui/base/BaseDisplaySnapshot.h"
#include "ui/base/BuildingsDisplay.h"
#include "ui/style/UiStyle.h"
#include "ui/UIElement.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>

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
    CHECK(snapshot.mineralProduction == rBase.GetMineralsForProduction());

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

namespace
{

MouseEvent_t ClickHurryButton_(const WindowLayout_t& rScreen)
{
    const WindowLayout_t left = ResolveLayout(rScreen, Style().layouts.leftPanel);
    const WindowLayout_t queue = ResolveLayout(left, Style().baseView.buildQueueLayout);
    const WindowLayout_t hurry = ResolveLayout(queue, Style().buildQueueDisplay.hurryButtonLayout);
    return MouseEvent_t{
        MouseButton_t::Left,
        static_cast<int>(hurry.x + hurry.width * 0.5f),
        static_cast<int>(hurry.y + hurry.height * 0.5f),
        {},
        true};
}

const BuildingConfig_t* QueueHurryFacility_(BaseManager& rBase, ViewFixture& rFixture)
{
    const BuildingConfig_t* pFacility = rFixture.dataContext.buildingRegistry->Find("test_hurry_facility");
    REQUIRE(pFacility != nullptr);
    rBase.GetProduction().SetProduction(pFacility);
    rBase.GetProduction().SetMineralStockpile(0);
    return pFacility;
}

MouseEvent_t ClickAtDrawnText_(const RecordingGraphics& rGraphics, const std::string& rLabel)
{
    const RecordingGraphics::TextDraw_t* pDraw = nullptr;
    for (const RecordingGraphics::TextDraw_t& rDraw : rGraphics.texts)
    {
        if (rDraw.text == rLabel)
        {
            pDraw = &rDraw;
            break;
        }
    }
    REQUIRE(pDraw != nullptr);
    return MouseEvent_t{
        MouseButton_t::Left,
        static_cast<int>(pDraw->x + 4.0f),
        static_cast<int>(pDraw->y + 2.0f),
        {},
        true};
}

} // namespace

TEST_CASE("BaseView draws Hurry on the build queue", "[ui][base][hurry]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("Build Queue"));
    CHECK(fixture.graphics.AnyTextContaining("Hurry"));
}

TEST_CASE("Hurry does nothing when the queued item cannot be hurried", "[ui][base][hurry]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    REQUIRE_FALSE(rBase.QuoteHurry().bAvailable);

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->HandleMouse(ClickHurryButton_(ViewFixture::FullScreen()));
    CHECK_FALSE(pView->HasModalElement());
}

TEST_CASE("Clicking Hurry opens a popup quoting the finish cost", "[ui][base][hurry]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    QueueHurryFacility_(rBase, fixture);

    const HurryQuote_t quote = rBase.QuoteHurry();
    REQUIRE(quote.bAvailable);
    REQUIRE(quote.creditCost > 0);

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->HandleMouse(ClickHurryButton_(ViewFixture::FullScreen()));
    CHECK(pView->HasModalElement());

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining(
        "Finishing construction costs " + std::to_string(quote.creditCost) + " credits."));
    CHECK(fixture.graphics.AnyTextContaining(std::to_string(quote.creditCost)));
    CHECK(fixture.graphics.AnyTextContaining("OK"));
    CHECK(fixture.graphics.AnyTextContaining("Cancel"));
}

TEST_CASE("Confirming Hurry spends the quoted credits", "[ui][base][hurry]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    QueueHurryFacility_(rBase, fixture);
    const HurryQuote_t quote = rBase.QuoteHurry();
    REQUIRE(quote.creditCost > 0);
    rBase.GetFaction().GetEconomy().AddEnergy(quote.creditCost);

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->HandleMouse(ClickHurryButton_(ViewFixture::FullScreen()));
    CHECK(pView->HandleKey(KeyEvent_t{Key_t::Enter, {}}));
    CHECK_FALSE(pView->HasModalElement());
    CHECK(rBase.GetFaction().GetEconomy().GetEnergy() == 0);
}

TEST_CASE("Hurry that the treasury cannot cover explains why rather than throwing",
          "[ui][base][hurry]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    QueueHurryFacility_(rBase, fixture);
    REQUIRE(rBase.QuoteHurry().creditCost > 1);
    rBase.GetFaction().GetEconomy().AddEnergy(1);

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->HandleMouse(ClickHurryButton_(ViewFixture::FullScreen()));
    CHECK(pView->HandleKey(KeyEvent_t{Key_t::Enter, {}}));
    CHECK(pView->HasModalElement());
    CHECK(rBase.GetFaction().GetEconomy().GetEnergy() == 1);

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("Not enough energy credits."));
}

TEST_CASE("A stockpile queue shows no turns to completion", "[ui][base][production]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    CHECK_FALSE(rBase.GetTurnsToProductionCompletion().has_value());

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("Turns: -"));
}

TEST_CASE("BaseView shows turns to completion for a queued facility", "[ui][base][production]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    QueueHurryFacility_(rBase, fixture);
    rBase.GetBuildingManager().AddBuilding("mineral_cache");

    const std::optional<int> turns = rBase.GetTurnsToProductionCompletion();
    REQUIRE(turns.has_value());
    REQUIRE(*turns >= 1);

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("Turns: " + std::to_string(*turns)));
}

TEST_CASE("Clicking a constructed building offers scrap here or scrap everywhere",
          "[ui][base][scrap]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    rBase.GetBuildingManager().AddBuilding("test_hurry_facility");

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Test Hurry Facility"));
    CHECK(pView->HasModalElement());

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("Test Hurry Facility"));
    CHECK(fixture.graphics.AnyTextContaining("Scrap Building"));
    CHECK(fixture.graphics.AnyTextContaining("Scrap this building at all bases"));
}

TEST_CASE("Dismissing the scrap menu leaves the building in place", "[ui][base][scrap]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    rBase.GetBuildingManager().AddBuilding("test_hurry_facility");

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Test Hurry Facility"));
    CHECK(pView->HandleKey(KeyEvent_t{Key_t::Escape, {}}));
    CHECK_FALSE(pView->HasModalElement());
    CHECK(rBase.GetBuildingManager().HasBuilding("test_hurry_facility"));
    CHECK(rBase.GetFaction().GetEconomy().GetEnergy() == 0);
}

TEST_CASE("Confirming scrap of one copy quotes the refund and then grants it",
          "[ui][base][scrap]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    rBase.GetBuildingManager().AddBuilding("test_hurry_facility");

    const auto payout = rBase.GetFaction().QuoteScrapBuilding(rBase, "test_hurry_facility");
    REQUIRE(payout.has_value());
    REQUIRE(payout->amount == 10);

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Test Hurry Facility"));
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Scrap Building"));
    CHECK(pView->HasModalElement());

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("Refund 10 energy credits?"));
    CHECK(fixture.graphics.AnyTextContaining("OK"));
    CHECK(fixture.graphics.AnyTextContaining("Cancel"));

    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "OK"));
    CHECK_FALSE(pView->HasModalElement());
    CHECK_FALSE(rBase.GetBuildingManager().HasBuilding("test_hurry_facility"));
    CHECK(rBase.GetFaction().GetEconomy().GetEnergy() == 10);
}

TEST_CASE("Cancel on the scrap confirm leaves the building in place", "[ui][base][scrap]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    rBase.GetBuildingManager().AddBuilding("test_hurry_facility");

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Test Hurry Facility"));
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Scrap Building"));
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Cancel"));
    CHECK_FALSE(pView->HasModalElement());
    CHECK(rBase.GetBuildingManager().HasBuilding("test_hurry_facility"));
    CHECK(rBase.GetFaction().GetEconomy().GetEnergy() == 0);
}

TEST_CASE("Confirming scrap at all bases removes every copy and refunds each",
          "[ui][base][scrap]")
{
    ViewFixture fixture;
    BaseManager& rHere = fixture.MakeBase(2, 2);
    BaseManager& rThere = fixture.MakeBase(5, 5);
    rHere.GetBuildingManager().AddBuilding("test_hurry_facility");
    rThere.GetBuildingManager().AddBuilding("test_hurry_facility");

    auto pView = fixture.pFactory->CreateBaseView(rHere, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Test Hurry Facility"));
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Scrap this building at all bases"));

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("Refund 20 energy credits?"));

    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "OK"));
    CHECK_FALSE(rHere.GetBuildingManager().HasBuilding("test_hurry_facility"));
    CHECK_FALSE(rThere.GetBuildingManager().HasBuilding("test_hurry_facility"));
    CHECK(rHere.GetFaction().GetEconomy().GetEnergy() == 20);
}

TEST_CASE("A secret project explains that it cannot be scrapped instead of quoting a refund",
          "[ui][base][scrap][secret-project]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    rBase.GetBuildingManager().AddBuilding("test_secret_project");

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Test Secret Project"));
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Scrap Building"));
    CHECK(pView->HasModalElement());

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("This building cannot be scrapped."));
    CHECK(rBase.GetBuildingManager().HasBuilding("test_secret_project"));
    CHECK(rBase.GetFaction().GetEconomy().GetEnergy() == 0);
}

TEST_CASE("Headquarters explains that it cannot be scrapped instead of quoting a refund",
          "[ui][base][scrap][hq]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    REQUIRE(rBase.GetBuildingManager().HasBuilding("Headquarters"));

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Headquarters"));
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Scrap Building"));
    CHECK(pView->HasModalElement());

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("This building cannot be scrapped."));
    CHECK(rBase.GetBuildingManager().HasBuilding("Headquarters"));
    CHECK(rBase.GetFaction().GetEconomy().GetEnergy() == 0);
}

TEST_CASE("The scrap menu titles a granted copy by name, without the list's marker",
          "[ui][base][scrap]")
{
    // The buildings list marks a constructed copy that is also granted with "*". That marker is
    // list chrome, not the building's name, so the menu must not inherit it - which is what the
    // view's own second lookup of the name used to guard against by accident.
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    rBase.GetBuildingManager().AddBuilding("grantor_global");
    rBase.GetBuildingManager().AddBuilding("granted_hall");

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);
    // The marked form is the only one on screen while the list is all there is.
    REQUIRE(DrawnText_(fixture.graphics, "* Granted Hall") != nullptr);
    REQUIRE(DrawnText_(fixture.graphics, "Granted Hall") == nullptr);

    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "* Granted Hall"));
    REQUIRE(pView->HasModalElement());

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(DrawnText_(fixture.graphics, "Granted Hall") != nullptr);
    CHECK(fixture.graphics.AnyTextContaining("Scrap Building"));
}

TEST_CASE("Clicking a granted-only building offers scrap and then denies it",
          "[ui][base][scrap]")
{
    ViewFixture fixture;
    BaseManager& rGrantorBase = fixture.MakeBase(4, 4);
    BaseManager& rOtherBase = fixture.MakeBase(6, 6);
    rGrantorBase.GetBuildingManager().AddBuilding("grantor_global");
    REQUIRE_FALSE(rOtherBase.GetBuildingManager().HasBuilding("granted_hall"));

    auto pView = fixture.pFactory->CreateBaseView(rOtherBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Granted Hall"));
    CHECK(pView->HasModalElement());

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("Scrap Building"));
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Scrap Building"));
    CHECK(pView->HasModalElement());

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("This building cannot be scrapped."));
    CHECK_FALSE(rOtherBase.GetBuildingManager().HasBuilding("granted_hall"));
}

TEST_CASE("A Command Nexus grant offers scrap and then denies it", "[ui][base][scrap]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    rBase.GetBuildingManager().AddBuilding("Command_Nexus");
    REQUIRE_FALSE(rBase.GetBuildingManager().HasBuilding("Command_Center"));

    auto pView = fixture.pFactory->CreateBaseView(rBase, ViewFixture::FullScreen());
    REQUIRE(pView);
    pView->Render(fixture.graphics);
    pView->HandleMouse(
        ClickAtDrawnText_(fixture.graphics, "Command Center (+2 starting XP to land units)"));
    CHECK(pView->HasModalElement());

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Scrap Building"));

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("This building cannot be scrapped."));
    CHECK_FALSE(rBase.GetBuildingManager().HasBuilding("Command_Center"));
    CHECK(rBase.GetBuildingManager().HasBuilding("Command_Nexus"));
}

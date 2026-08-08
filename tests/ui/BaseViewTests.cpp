// BaseView's display snapshot: it must not recompute on every paint, and it must not go stale
// when the player acts. Package 15 commit B.

#include "ViewFixture.h"

#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "game/population/pop-types/Pop.h"
#include "ui/base/BaseDisplaySnapshot.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using actest::ViewFixture;

namespace
{

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

    for (const auto& [pTile, rEntry] : snapshot.tiles)
    {
        const TileYieldView_t live = rEntry.workState == TileWorkState_t::WorkedByThisBase
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

    // Repainting an unchanged base must not change what is drawn.
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.texts.size() == drawsPerFrame);

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
    CHECK(fixture.graphics.AnyTextContaining("Production: " + std::to_string(afterNutrients)));
    CHECK_FALSE(fixture.graphics.AnyTextContaining("Production: " + std::to_string(beforeNutrients)));
}

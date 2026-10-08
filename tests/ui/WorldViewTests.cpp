// World-map click priority: a tile with a base opens the base screen even when a garrison
// is stacked there. The base view's unit stack is how the player picks those units.

#include "ViewFixture.h"

#include "game/Faction.h"
#include "game/faction/UnitManager.h"
#include "game/faction/UnitVisibility.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitSlotConfig.h"
#include "input/Input.h"
#include "ui/IGameView.h"
#include "ui/UIElement.h"
#include "ui/style/UiStyle.h"
#include "ui/world/AirdropFailMessages.h"
#include "ui/world/WorldDisplay.h"
#include "ui/world/WorldView.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <deque>
#include <memory>
#include <string>
#include <variant>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace ac;
using actest::RecordingGraphics;
using actest::ViewFixture;

namespace
{

Unit& MakeUnit_(ViewFixture& rFixture, Faction& rOwner, int x, int y, BaseManager* pHome,
                const std::vector<std::string>& rComponentIds,
                std::deque<UnitDesign>& rDesigns)
{
    std::vector<UnitSlotConfig_t> slots;
    std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
    int slotIndex = 0;
    for (const std::string& rId : rComponentIds)
    {
        const UnitComponentConfig_t* pComponent = rFixture.unitComponents.Find(rId);
        REQUIRE(pComponent);
        UnitSlotConfig_t slot;
        slot.id = "slot_" + std::to_string(slotIndex++);
        slot.displayName = slot.id;
        slot.componentType = pComponent->type;
        slots.push_back(slot);
        assigned[slot.id] = pComponent;
    }
    rDesigns.emplace_back(slots, assigned);

    Tile* pTile = rFixture.pState->GetWorldMap().GetTile(x, y);
    REQUIRE(pTile);
    return rOwner.GetUnitManager().CreateUnit(
        rFixture.pState->AllocateUnitId(), rDesigns.back(),
        rFixture.pState->GetWorldMap().GetUnitPositions(), *pTile, pHome);
}

Unit& MakeUnit_(ViewFixture& rFixture, int x, int y, BaseManager* pHome,
                const std::vector<std::string>& rComponentIds,
                std::deque<UnitDesign>& rDesigns)
{
    return MakeUnit_(rFixture, *rFixture.pPlayer, x, y, pHome, rComponentIds, rDesigns);
}

Unit& MakeGarrison_(ViewFixture& rFixture, int x, int y, BaseManager* pHome,
                    std::deque<UnitDesign>& rDesigns)
{
    return MakeUnit_(rFixture, x, y, pHome, {"test_chassis", "test_armor"}, rDesigns);
}

bool UnitStillLive_(const Faction& rFaction, UnitId_t unitId)
{
    for (const Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        if (rUnit.GetUnitId() == unitId)
        {
            return true;
        }
    }
    return false;
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

std::unique_ptr<WorldView> MakeWorldView_(ViewFixture& rFixture)
{
    return rFixture.pFactory->CreateWorldView(
        ViewFixture::FullScreen(), [] {}, [] {},
        [](BaseManager&) {}, [](auto&&...) {}, [] {});
}

// Auto-select and panel wiring live on UpdatePresentation (Engine's Update path), not Render.
void PrimeWorldView_(WorldView& rView, RecordingGraphics& rGraphics)
{
    rView.UpdatePresentation();
    rView.Render(rGraphics);
}

KeyEvent_t ShiftD_()
{
    return KeyEvent_t{Key_t::D, ModifierState_t{false, false, true}};
}

KeyEvent_t PlainKey_(Key_t key)
{
    return KeyEvent_t{key, {}};
}

MouseEvent_t PressAt_(int x, int y)
{
    return MouseEvent_t{MouseButton_t::Left, x, y, {}, true};
}

MouseEvent_t ReleaseAt_(int x, int y)
{
    return MouseEvent_t{MouseButton_t::Left, x, y, {}, /*bPressed*/ false};
}

// Pixel center of a tile's diamond in the brick layout. Camera starts at (0,0); FullScreen +
// style map layout put SMAC tile (tileX, tileY) at ((tileX)·½w, (tileY)·½h) from the map origin.
std::pair<int, int> MapTileClick_(const WindowLayout_t& rFullscreen, int tileX, int tileY)
{
    const WindowLayout_t mapLayout = ResolveLayout(rFullscreen, Style().layouts.map);
    const float tileWidth = mapLayout.height * Style().worldDisplay.defaultTileScale;
    const float tileHeight = tileWidth * 0.5f;
    const float halfW = tileWidth * 0.5f;
    const float halfH = tileHeight * 0.5f;
    const float x = mapLayout.x + static_cast<float>(tileX) * halfW + halfW;
    const float y = mapLayout.y + static_cast<float>(tileY) * halfH + halfH;
    return {static_cast<int>(x), static_cast<int>(y)};
}

} // namespace

TEST_CASE("Clicking a garrisoned base opens the base view", "[ui][world]")
{
    ViewFixture fixture;
    const WindowLayout_t layout = ViewFixture::FullScreen();
    BaseManager& rBase = fixture.MakeBase(8, 8);
    fixture.pPlayer->GetExploredMap().MarkAll();

    std::deque<UnitDesign> designs;
    MakeGarrison_(fixture, 8, 8, &rBase, designs);

    BaseManager* pOpened = nullptr;
    auto pView = fixture.pFactory->CreateWorldView(
        layout, [] {}, [] {},
        [&](BaseManager& rOpened) { pOpened = &rOpened; },
        [](auto&&...) {}, [] {});

    const auto [x, y] = MapTileClick_(layout, 8, 8);
    pView->HandleMouse(ReleaseAt_(x, y));

    REQUIRE(pOpened == &rBase);
}

TEST_CASE("Clicking a unit off a base selects it without opening a base", "[ui][world]")
{
    ViewFixture fixture;
    const WindowLayout_t layout = ViewFixture::FullScreen();
    fixture.MakeBase(8, 2);
    fixture.pPlayer->GetExploredMap().MarkAll();

    std::deque<UnitDesign> designs;
    MakeGarrison_(fixture, 8, 8, nullptr, designs);

    bool bOpenedBase = false;
    auto pView = fixture.pFactory->CreateWorldView(
        layout, [] {}, [] {},
        [&](BaseManager&) { bOpenedBase = true; },
        [](auto&&...) {}, [] {});

    const auto [x, y] = MapTileClick_(layout, 8, 8);
    pView->HandleMouse(ReleaseAt_(x, y));

    CHECK_FALSE(bOpenedBase);
}

TEST_CASE("Shift+D on a selected unit opens Disband, Self Destruct, and Cancel",
          "[ui][world][disband]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(8, 8);
    std::deque<UnitDesign> designs;
    MakeUnit_(fixture, 8, 8, &rBase, {"test_chassis", "test_costly_weapon"}, designs);

    auto pView = MakeWorldView_(fixture);
    PrimeWorldView_(*pView, fixture.graphics);
    CHECK(pView->HandleKey(ShiftD_()));
    CHECK(pView->HasModalElement());

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("Disband Units"));
    CHECK(fixture.graphics.AnyTextContaining("Disband"));
    CHECK(fixture.graphics.AnyTextContaining("Self Destruct"));
    CHECK(fixture.graphics.AnyTextContaining("Cancel"));
}

TEST_CASE("Shift+D does nothing when no unit is selected", "[ui][world][disband]")
{
    ViewFixture fixture;
    auto pView = MakeWorldView_(fixture);
    PrimeWorldView_(*pView, fixture.graphics);
    CHECK_FALSE(pView->HandleKey(ShiftD_()));
    CHECK_FALSE(pView->HasModalElement());
}

TEST_CASE("Plain D does not open the disband menu", "[ui][world][disband]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(8, 8);
    std::deque<UnitDesign> designs;
    MakeUnit_(fixture, 8, 8, &rBase, {"test_chassis", "test_costly_weapon"}, designs);

    auto pView = MakeWorldView_(fixture);
    PrimeWorldView_(*pView, fixture.graphics);
    pView->HandleKey(KeyEvent_t{Key_t::D, {}});
    CHECK_FALSE(pView->HasModalElement());
}

TEST_CASE("Confirming Disband quotes the refund and then grants it", "[ui][world][disband]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(8, 8);
    std::deque<UnitDesign> designs;
    Unit& rUnit =
        MakeUnit_(fixture, 8, 8, &rBase, {"test_chassis", "test_costly_weapon"}, designs);
    const UnitId_t unitId = rUnit.GetUnitId();
    const auto payout = fixture.pPlayer->QuoteScrapUnit(rUnit);
    REQUIRE(payout.has_value());
    REQUIRE(payout->amount == 10);
    REQUIRE(rBase.GetProduction().GetMineralStockpile() == 0);

    auto pView = MakeWorldView_(fixture);
    PrimeWorldView_(*pView, fixture.graphics);
    REQUIRE(pView->HandleKey(ShiftD_()));
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Disband"));
    CHECK(pView->HasModalElement());

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("Refund 10 minerals to TestBase?"));
    CHECK(fixture.graphics.AnyTextContaining("OK"));
    CHECK(fixture.graphics.AnyTextContaining("Cancel"));

    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "OK"));
    CHECK_FALSE(pView->HasModalElement());
    CHECK_FALSE(UnitStillLive_(*fixture.pPlayer, unitId));
    CHECK(rBase.GetProduction().GetMineralStockpile() == 10);
}

TEST_CASE("Cancel on the disband menu leaves the unit in place", "[ui][world][disband]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(8, 8);
    std::deque<UnitDesign> designs;
    Unit& rUnit =
        MakeUnit_(fixture, 8, 8, &rBase, {"test_chassis", "test_costly_weapon"}, designs);
    const UnitId_t unitId = rUnit.GetUnitId();

    auto pView = MakeWorldView_(fixture);
    PrimeWorldView_(*pView, fixture.graphics);
    REQUIRE(pView->HandleKey(ShiftD_()));
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Cancel"));
    CHECK_FALSE(pView->HasModalElement());
    CHECK(UnitStillLive_(*fixture.pPlayer, unitId));
    CHECK(rBase.GetProduction().GetMineralStockpile() == 0);
}

TEST_CASE("Cancel on the disband confirm leaves the unit in place", "[ui][world][disband]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(8, 8);
    std::deque<UnitDesign> designs;
    Unit& rUnit =
        MakeUnit_(fixture, 8, 8, &rBase, {"test_chassis", "test_costly_weapon"}, designs);
    const UnitId_t unitId = rUnit.GetUnitId();

    auto pView = MakeWorldView_(fixture);
    PrimeWorldView_(*pView, fixture.graphics);
    REQUIRE(pView->HandleKey(ShiftD_()));
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Disband"));
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Cancel"));
    CHECK_FALSE(pView->HasModalElement());
    CHECK(UnitStillLive_(*fixture.pPlayer, unitId));
    CHECK(rBase.GetProduction().GetMineralStockpile() == 0);
}

TEST_CASE("Self Destruct explains that it is not implemented and leaves the unit",
          "[ui][world][disband]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(8, 8);
    std::deque<UnitDesign> designs;
    Unit& rUnit =
        MakeUnit_(fixture, 8, 8, &rBase, {"test_chassis", "test_costly_weapon"}, designs);
    const UnitId_t unitId = rUnit.GetUnitId();

    auto pView = MakeWorldView_(fixture);
    PrimeWorldView_(*pView, fixture.graphics);
    REQUIRE(pView->HandleKey(ShiftD_()));
    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    pView->HandleMouse(ClickAtDrawnText_(fixture.graphics, "Self Destruct"));
    CHECK(pView->HasModalElement());

    fixture.graphics.texts.clear();
    pView->Render(fixture.graphics);
    CHECK(fixture.graphics.AnyTextContaining("Self Destruct is not implemented."));
    CHECK(UnitStillLive_(*fixture.pPlayer, unitId));
    CHECK(rBase.GetProduction().GetMineralStockpile() == 0);
}

TEST_CASE("A shrouded unit is drawn only while bombard playback lists it", "[ui][world]")
{
    // Declared first so ~Faction runs while this config is still alive.
    FactionConfig_t enemyDefinition;
    ViewFixture fixture;
    std::deque<UnitDesign> designs;
    MakeUnit_(fixture, 8, 0, nullptr, {"test_chassis"}, designs);

    enemyDefinition = fixture.factionDefinition;
    enemyDefinition.id = "enemy_faction";
    enemyDefinition.identity.name = "Enemy";
    Faction& enemy = fixture.pState->AddFaction(std::make_unique<Faction>(
        fixture.pState->AllocateFactionId(), false, enemyDefinition, fixture.dataContext,
        fixture.pState->GetWorldMap(), fixture.settings, actest::k_TestFactionSeed + 1));
    Unit& shrouded = MakeUnit_(fixture, enemy, 10, 2, nullptr, {"test_chassis"}, designs);
    fixture.pPlayer->RebuildVisibility();
    CHECK_FALSE(IsUnitVisibleTo(*fixture.pPlayer, shrouded, fixture.pState->GetTileEffects()));

    WorldDisplay display(*fixture.pState, *fixture.pMapRenderer, ViewFixture::FullScreen());
    display.Render(fixture.graphics);
    CHECK_FALSE(display.MarkerRectOf(shrouded.GetUnitId()).has_value());

    const std::unordered_set<UnitId_t> playback{shrouded.GetUnitId()};
    display.SetPlaybackVisibleUnits(&playback);
    display.Render(fixture.graphics);
    CHECK(display.MarkerRectOf(shrouded.GetUnitId()).has_value());

    display.SetPlaybackVisibleUnits(nullptr);
    display.Render(fixture.graphics);
    CHECK_FALSE(display.MarkerRectOf(shrouded.GetUnitId()).has_value());
    CHECK_FALSE(fixture.pPlayer->GetRevealedUnits().IsRevealed(shrouded));
}

TEST_CASE("F builds a Farm for a former", "[ui][world][bombard]")
{
    ViewFixture fixture;
    fixture.pPlayer->GetEconomy().AddEnergy(100);
    Tile* pTile = fixture.pState->GetWorldMap().GetTile(8, 8);
    REQUIRE(pTile);
    pTile->SetElevation(100);
    pTile->SetRockiness(Rockiness_t::Flat);

    std::deque<UnitDesign> designs;
    Unit& former = MakeUnit_(fixture, 8, 8, nullptr, {"test_chassis", "test_terraformer"}, designs);
    auto pView = MakeWorldView_(fixture);
    PrimeWorldView_(*pView, fixture.graphics);

    REQUIRE(pView->HandleKey(PlainKey_(Key_t::F)));
    REQUIRE(former.GetOrder().has_value());
    REQUIRE(std::holds_alternative<TerraformOrder_t>(*former.GetOrder()));
    CHECK(std::get<TerraformOrder_t>(*former.GetOrder()).projectId == "Farm");
}

TEST_CASE("F arms bombard when the unit can fire, and the next click shoots", "[ui][world][bombard]")
{
    ViewFixture fixture;
    const WindowLayout_t layout = ViewFixture::FullScreen();
    std::deque<UnitDesign> designs;
    Unit& artillery = MakeUnit_(fixture, 8, 0, nullptr, {"test_chassis", "bombard"}, designs);
    const int fragmentsBefore = artillery.GetMoveFragmentsRemaining();
    REQUIRE(fragmentsBefore > 0);

    bool bOpened = false;
    auto pView = fixture.pFactory->CreateWorldView(
        layout, [] {}, [] {}, [](BaseManager&) {},
        [&](auto&&...) { bOpened = true; }, [] {});
    fixture.pPlayer->GetExploredMap().MarkAll();
    const auto [unitX, unitY] = MapTileClick_(layout, 8, 0);
    pView->HandleMouse(ReleaseAt_(unitX, unitY));

    REQUIRE(pView->HandleKey(PlainKey_(Key_t::F)));
    CHECK_FALSE(artillery.GetOrder().has_value());

    const auto [farX, farY] = MapTileClick_(layout, 12, 4);
    pView->HandleMouse(PressAt_(farX, farY));
    CHECK_FALSE(bOpened);
    CHECK(artillery.GetMoveFragmentsRemaining() == fragmentsBefore);

    const auto [nearX, nearY] = MapTileClick_(layout, 9, 1);
    pView->HandleMouse(PressAt_(nearX, nearY));
    CHECK(bOpened);
    CHECK(artillery.GetMoveFragmentsRemaining() == 0);
}

TEST_CASE("F throws when bombard and Farm are both valid", "[ui][world][bombard]")
{
    ViewFixture fixture;
    fixture.pPlayer->GetEconomy().AddEnergy(100);
    Tile* pTile = fixture.pState->GetWorldMap().GetTile(8, 0);
    REQUIRE(pTile);
    pTile->SetElevation(100);
    pTile->SetRockiness(Rockiness_t::Flat);

    const WindowLayout_t layout = ViewFixture::FullScreen();
    std::deque<UnitDesign> designs;
    Unit& unit = MakeUnit_(
        fixture, 8, 0, nullptr, {"test_chassis", "test_terraformer", "bombard"}, designs);

    auto pView = fixture.pFactory->CreateWorldView(
        layout, [] {}, [] {}, [](BaseManager&) {}, [](auto&&...) {}, [] {});
    fixture.pPlayer->GetExploredMap().MarkAll();
    const auto [unitX, unitY] = MapTileClick_(layout, 8, 0);
    pView->HandleMouse(ReleaseAt_(unitX, unitY));

    CHECK_THROWS_WITH(pView->HandleKey(PlainKey_(Key_t::F)),
                      Catch::Matchers::ContainsSubstring("bombard")
                          && Catch::Matchers::ContainsSubstring("Farm"));
    CHECK_FALSE(unit.GetOrder().has_value());
}

TEST_CASE("F with no moves left does not arm bombard", "[ui][world][bombard]")
{
    ViewFixture fixture;
    const WindowLayout_t layout = ViewFixture::FullScreen();
    std::deque<UnitDesign> designs;
    Unit& artillery = MakeUnit_(fixture, 8, 0, nullptr, {"test_chassis", "bombard"}, designs);
    artillery.SetMoveFragmentsRemaining(0);

    bool bOpened = false;
    auto pView = fixture.pFactory->CreateWorldView(
        layout, [] {}, [] {}, [](BaseManager&) {},
        [&](auto&&...) { bOpened = true; }, [] {});
    fixture.pPlayer->GetExploredMap().MarkAll();
    const auto [unitX, unitY] = MapTileClick_(layout, 8, 0);
    pView->HandleMouse(ReleaseAt_(unitX, unitY));

    pView->HandleKey(PlainKey_(Key_t::F));
    const auto [x, y] = MapTileClick_(layout, 9, 1);
    pView->HandleMouse(PressAt_(x, y));
    CHECK_FALSE(bOpened);
    CHECK_FALSE(artillery.GetOrder().has_value());
}

TEST_CASE("AirdropFailReasonMessage covers interdiction and occupation denies",
          "[ui][world][airdrop]")
{
    CHECK_FALSE(AirdropFailReasonMessage(AirdropFailReason_t::Interdicted).empty());
    CHECK_FALSE(AirdropFailReasonMessage(AirdropFailReason_t::EnemyOccupied).empty());
}

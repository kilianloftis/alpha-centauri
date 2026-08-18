// World-map click priority: a tile with a base opens the base screen even when a garrison
// is stacked there. The base view's unit stack is how the player picks those units.

#include "ViewFixture.h"

#include "game/Faction.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"
#include "input/Input.h"
#include "ui/IGameView.h"
#include "ui/UIElement.h"
#include "ui/style/UiStyle.h"
#include "ui/world/WorldView.h"

#include <catch2/catch_test_macros.hpp>

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ac;
using actest::RecordingGraphics;
using actest::ViewFixture;

namespace
{

Unit& MakeUnit_(ViewFixture& rFixture, int x, int y, BaseManager* pHome,
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
    return rFixture.pPlayer->GetUnitManager().CreateUnit(
        rFixture.pState->AllocateUnitId(), rDesigns.back(),
        rFixture.pState->GetWorldMap().GetUnitPositions(), *pTile, pHome);
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

std::unique_ptr<IGameView> MakeWorldView_(ViewFixture& rFixture)
{
    return rFixture.pFactory->CreateWorldView(
        ViewFixture::FullScreen(), [] {}, [] {},
        [](BaseManager&) {}, [](auto&&...) {}, [] {});
}

KeyEvent_t ShiftD_()
{
    return KeyEvent_t{Key_t::D, ModifierState_t{false, false, true}};
}

MouseEvent_t ReleaseAt_(int x, int y)
{
    return MouseEvent_t{MouseButton_t::Left, x, y, {}, /*bPressed*/ false};
}

// Pixel center of a camera-relative map cell. Camera starts at (0,0); FullScreen + style
// map layout put world tile (tileX, tileY) in that cell for the 9×9 fixture map.
std::pair<int, int> MapTileClick_(const WindowLayout_t& rFullscreen, int tileX, int tileY)
{
    const WindowLayout_t mapLayout = ResolveLayout(rFullscreen, Style().layouts.map);
    const float tileSize = mapLayout.height * Style().worldDisplay.defaultTileScale;
    const int x = static_cast<int>(mapLayout.x + (static_cast<float>(tileX) + 0.5f) * tileSize);
    const int y = static_cast<int>(mapLayout.y + (static_cast<float>(tileY) + 0.5f) * tileSize);
    return {x, y};
}

} // namespace

TEST_CASE("Clicking a garrisoned base opens the base view", "[ui][world]")
{
    ViewFixture fixture;
    const WindowLayout_t layout = ViewFixture::FullScreen();
    BaseManager& rBase = fixture.MakeBase(4, 4);
    fixture.pPlayer->GetExploredMap().MarkAll();

    std::deque<UnitDesign> designs;
    MakeGarrison_(fixture, 4, 4, &rBase, designs);

    BaseManager* pOpened = nullptr;
    auto pView = fixture.pFactory->CreateWorldView(
        layout, [] {}, [] {},
        [&](BaseManager& rOpened) { pOpened = &rOpened; },
        [](auto&&...) {}, [] {});

    const auto [x, y] = MapTileClick_(layout, 4, 4);
    pView->HandleMouse(ReleaseAt_(x, y));

    REQUIRE(pOpened == &rBase);
}

TEST_CASE("Clicking a unit off a base selects it without opening a base", "[ui][world]")
{
    ViewFixture fixture;
    const WindowLayout_t layout = ViewFixture::FullScreen();
    fixture.MakeBase(1, 1);
    fixture.pPlayer->GetExploredMap().MarkAll();

    std::deque<UnitDesign> designs;
    MakeGarrison_(fixture, 4, 4, nullptr, designs);

    bool bOpenedBase = false;
    auto pView = fixture.pFactory->CreateWorldView(
        layout, [] {}, [] {},
        [&](BaseManager&) { bOpenedBase = true; },
        [](auto&&...) {}, [] {});

    const auto [x, y] = MapTileClick_(layout, 4, 4);
    pView->HandleMouse(ReleaseAt_(x, y));

    CHECK_FALSE(bOpenedBase);
}

TEST_CASE("Shift+D on a selected unit opens Disband, Self Destruct, and Cancel",
          "[ui][world][disband]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    std::deque<UnitDesign> designs;
    MakeUnit_(fixture, 4, 4, &rBase, {"test_chassis", "test_costly_weapon"}, designs);

    auto pView = MakeWorldView_(fixture);
    pView->Render(fixture.graphics);
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
    pView->Render(fixture.graphics);
    CHECK_FALSE(pView->HandleKey(ShiftD_()));
    CHECK_FALSE(pView->HasModalElement());
}

TEST_CASE("Plain D does not open the disband menu", "[ui][world][disband]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    std::deque<UnitDesign> designs;
    MakeUnit_(fixture, 4, 4, &rBase, {"test_chassis", "test_costly_weapon"}, designs);

    auto pView = MakeWorldView_(fixture);
    pView->Render(fixture.graphics);
    pView->HandleKey(KeyEvent_t{Key_t::D, {}});
    CHECK_FALSE(pView->HasModalElement());
}

TEST_CASE("Confirming Disband quotes the refund and then grants it", "[ui][world][disband]")
{
    ViewFixture fixture;
    BaseManager& rBase = fixture.MakeBase(4, 4);
    std::deque<UnitDesign> designs;
    Unit& rUnit =
        MakeUnit_(fixture, 4, 4, &rBase, {"test_chassis", "test_costly_weapon"}, designs);
    const UnitId_t unitId = rUnit.GetUnitId();
    const auto payout = fixture.pPlayer->QuoteScrapUnit(rUnit);
    REQUIRE(payout.has_value());
    REQUIRE(payout->amount == 10);
    REQUIRE(rBase.GetProduction().GetMineralStockpile() == 0);

    auto pView = MakeWorldView_(fixture);
    pView->Render(fixture.graphics);
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
    BaseManager& rBase = fixture.MakeBase(4, 4);
    std::deque<UnitDesign> designs;
    Unit& rUnit =
        MakeUnit_(fixture, 4, 4, &rBase, {"test_chassis", "test_costly_weapon"}, designs);
    const UnitId_t unitId = rUnit.GetUnitId();

    auto pView = MakeWorldView_(fixture);
    pView->Render(fixture.graphics);
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
    BaseManager& rBase = fixture.MakeBase(4, 4);
    std::deque<UnitDesign> designs;
    Unit& rUnit =
        MakeUnit_(fixture, 4, 4, &rBase, {"test_chassis", "test_costly_weapon"}, designs);
    const UnitId_t unitId = rUnit.GetUnitId();

    auto pView = MakeWorldView_(fixture);
    pView->Render(fixture.graphics);
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
    BaseManager& rBase = fixture.MakeBase(4, 4);
    std::deque<UnitDesign> designs;
    Unit& rUnit =
        MakeUnit_(fixture, 4, 4, &rBase, {"test_chassis", "test_costly_weapon"}, designs);
    const UnitId_t unitId = rUnit.GetUnitId();

    auto pView = MakeWorldView_(fixture);
    pView->Render(fixture.graphics);
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

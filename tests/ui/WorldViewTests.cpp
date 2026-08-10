// World-map click priority: a tile with a base opens the base screen even when a garrison
// is stacked there. The base view's unit stack is how the player picks those units.

#include "ViewFixture.h"

#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"
#include "input/Input.h"
#include "ui/UIElement.h"
#include "ui/style/UiStyle.h"
#include "ui/world/WorldView.h"

#include <catch2/catch_test_macros.hpp>

#include <deque>
#include <unordered_map>
#include <vector>

using namespace ac;
using actest::ViewFixture;

namespace
{

Unit& MakeGarrison_(ViewFixture& rFixture, int x, int y, BaseManager* pHome,
                    std::deque<UnitDesign>& rDesigns)
{
    std::vector<UnitSlotConfig_t> slots;
    std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
    int slotIndex = 0;
    for (const std::string& rId : {"test_chassis", "test_armor"})
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

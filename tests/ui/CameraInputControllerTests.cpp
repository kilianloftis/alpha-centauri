#include "ViewFixture.h"

#include "game/map/WorldMap.h"
#include "ui/HotkeyConfig.h"
#include "ui/style/UiStyle.h"
#include "ui/world/CameraInputController.h"
#include "ui/world/MapViewport.h"
#include "ui/world/WorldDisplay.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;
using actest::ViewFixture;

namespace
{

constexpr int k_TallMapHeight = 100;

struct CameraRig_t
{
    ViewFixture fixture{false, k_TallMapHeight};
    WorldDisplay display{*fixture.pState, ViewFixture::FullScreen()};
    CameraInputController controller{display, fixture.pState->GetWorldMap(),
                                     ViewFixture::FullScreen(), *fixture.pHotkeys};

    MapViewport& Viewport() { return display.GetViewport(); }
    int MaxCameraY() const
    {
        return k_TallMapHeight - display.GetVisibleRows();
    }
    bool Press(Key_t key) { return controller.HandleKey(KeyEvent_t{key, ModifierState_t{}}); }
};

} // namespace

TEST_CASE("Camera pans move along the screen axes", "[ui][camera]")
{
    CameraRig_t rig;
    const int step = Style().cameraInput.cameraScrollStep * 2;
    rig.Viewport().SetCamera(10, 20);

    SECTION("right and left move only X")
    {
        CHECK(rig.Press(Key_t::ArrowRight));
        CHECK(rig.Viewport().CameraX() == 10 + step);
        CHECK(rig.Viewport().CameraY() == 20);

        CHECK(rig.Press(Key_t::ArrowLeft));
        CHECK(rig.Press(Key_t::ArrowLeft));
        CHECK(rig.Viewport().CameraX() == 10 - step);
        CHECK(rig.Viewport().CameraY() == 20);
    }

    SECTION("down and up move only Y")
    {
        CHECK(rig.Press(Key_t::ArrowDown));
        CHECK(rig.Viewport().CameraX() == 10);
        CHECK(rig.Viewport().CameraY() == 20 + step);

        CHECK(rig.Press(Key_t::ArrowUp));
        CHECK(rig.Press(Key_t::ArrowUp));
        CHECK(rig.Viewport().CameraX() == 10);
        CHECK(rig.Viewport().CameraY() == 20 - step);
    }

    SECTION("panning west across the seam wraps X")
    {
        rig.Viewport().SetCamera(0, 20);
        CHECK(rig.Press(Key_t::ArrowLeft));
        CHECK(rig.Viewport().CameraX() == rig.fixture.pState->GetWorldMap().GetWidth() - step);
    }
}

TEST_CASE("Camera vertical clamp stops at the poles", "[ui][camera]")
{
    CameraRig_t rig;
    const int maxCameraY = rig.MaxCameraY();
    REQUIRE(maxCameraY > 2);

    SECTION("north pole")
    {
        rig.Viewport().SetCamera(10, 0);
        CHECK_FALSE(rig.Press(Key_t::ArrowUp));
        CHECK(rig.Viewport().CameraY() == 0);

        rig.Viewport().SetCamera(10, 1);
        CHECK(rig.Press(Key_t::ArrowUp));
        CHECK(rig.Viewport().CameraY() == 0);
    }

    SECTION("south pole")
    {
        rig.Viewport().SetCamera(10, maxCameraY);
        CHECK_FALSE(rig.Press(Key_t::ArrowDown));
        CHECK(rig.Viewport().CameraY() == maxCameraY);

        rig.Viewport().SetCamera(10, maxCameraY - 1);
        CHECK(rig.Press(Key_t::ArrowDown));
        CHECK(rig.Viewport().CameraY() == maxCameraY);
    }

    SECTION("centering near a pole clamps Y but not X")
    {
        CHECK(rig.controller.CenterOnTile(20, 0));
        CHECK(rig.Viewport().CameraY() == 0);
        CHECK(rig.Viewport().CameraX() == 20 - rig.Viewport().VisibleCols() / 2);

        CHECK(rig.controller.CenterOnTile(20, k_TallMapHeight - 1));
        CHECK(rig.Viewport().CameraY() == maxCameraY);
    }
}

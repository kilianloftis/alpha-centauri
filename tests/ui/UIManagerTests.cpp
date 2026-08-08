// UIManager's own rules — push/pop, when closed views stop receiving input, the turn gate.
// Testable because UIManager depends on IWorldView rather than the concrete WorldView, which
// would drag in GameState, every registry, and the map renderer.

#include "RecordingGraphics.h"

#include "input/Input.h"
#include "input/PlatformEventQueue.h"
#include "ui/IWorldView.h"
#include "ui/UIElement.h"
#include "ui/UIManager.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <stdexcept>

using namespace ac;
using actest::RecordingGraphics;

namespace
{

class FakeWorldView_ : public IWorldView
{
public:
    FakeWorldView_()
        : IWorldView(WindowLayout_t{0.0f, 0.0f, 100.0f, 100.0f})
    {
    }

    void Render(Graphics&) override { ++renderCount; }

    void UpdateCameraInput(bool bEnabled, std::optional<MousePosition_t> mousePosition) override
    {
        ++cameraUpdateCount;
        bLastCameraEnabled = bEnabled;
        lastMousePosition = mousePosition;
    }

    void ProcessPendingAutoEndTurn() override { ++autoEndTurnCount; }

    bool BlocksTurnAdvance() const override { return bBlocksTurn; }

    int renderCount = 0;
    int cameraUpdateCount = 0;
    int autoEndTurnCount = 0;
    bool bLastCameraEnabled = false;
    std::optional<MousePosition_t> lastMousePosition;
    bool bBlocksTurn = false;
};

class FakeOverlay_ : public IGameView
{
public:
    FakeOverlay_()
        : IGameView(WindowLayout_t{0.0f, 0.0f, 100.0f, 100.0f})
    {
    }

    void Render(Graphics&) override { ++renderCount; }
    void OnPushed(Graphics&) override { ++pushCount; }
    void OnPopped() override { ++popCount; }

    bool HandleKey(const KeyEvent_t& rEvent) override
    {
        ++keyCount;
        if (rEvent.key == Key_t::Escape)
        {
            Close();
        }
        return true;
    }

    void HandleMouse(const MouseEvent_t&) override { ++mouseCount; }

    // IGameView keeps m_bShouldClose protected; a real view sets it from its own handlers.
    void Close() { m_bShouldClose = true; }

    int renderCount = 0;
    int pushCount = 0;
    int popCount = 0;
    int keyCount = 0;
    int mouseCount = 0;
};

// Bundles the manager with the queue its Input reads, so tests can inject events.
struct ManagerFixture_
{
    RecordingGraphics graphics;
    PlatformEventQueue queue;
    std::unique_ptr<Input> pInput = CreateInput(queue);
    UIManager manager{graphics, *pInput};

    FakeWorldView_* AddWorldView()
    {
        auto pWorld = std::make_unique<FakeWorldView_>();
        FakeWorldView_* pRaw = pWorld.get();
        manager.SetWorldView(std::move(pWorld));
        return pRaw;
    }

    FakeOverlay_* PushOverlay()
    {
        auto pOverlay = std::make_unique<FakeOverlay_>();
        FakeOverlay_* pRaw = pOverlay.get();
        manager.PushView(std::move(pOverlay));
        return pRaw;
    }
};

} // namespace

TEST_CASE("PushView refuses a null view instead of dereferencing it", "[ui][manager]")
{
    // Several Create*View methods returned nullptr when the player faction was missing, and
    // PushView called OnPushed on whatever it was handed.
    ManagerFixture_ fixture;
    CHECK_THROWS_AS(fixture.manager.PushView(nullptr), std::invalid_argument);
    CHECK_FALSE(fixture.manager.HasOverlayView());
}

TEST_CASE("A pushed view is notified once and becomes the active view", "[ui][manager]")
{
    ManagerFixture_ fixture;
    fixture.AddWorldView();
    FakeOverlay_* pOverlay = fixture.PushOverlay();

    CHECK(fixture.manager.HasOverlayView());
    CHECK(pOverlay->pushCount == 1);
    CHECK(pOverlay->popCount == 0);
}

TEST_CASE("A closed overlay stops receiving input before the next event is routed",
          "[ui][manager]")
{
    // Closed views were pruned inside Render, so between the close and the next paint the dead
    // overlay was still GetActiveView_() and kept receiving events drained in the same loop.
    ManagerFixture_ fixture;
    FakeWorldView_* pWorld = fixture.AddWorldView();
    FakeOverlay_* pOverlay = fixture.PushOverlay();

    // Escape closes the overlay; the click behind it in the same batch must not reach it.
    fixture.queue.PushKey({Key_t::Escape, {}});
    fixture.queue.PushMouse({MouseButton_t::Left, 5, 5, {}, true});
    fixture.manager.ProcessInput();

    CHECK(pOverlay->keyCount == 1);
    CHECK(pOverlay->mouseCount == 0);
    CHECK(pOverlay->popCount == 1);
    CHECK_FALSE(fixture.manager.HasOverlayView());
    // Routed to the world view instead, which is now active.
    CHECK(pWorld->renderCount == 0); // no render yet, only input
}

TEST_CASE("Pruning happens without a render", "[ui][manager]")
{
    ManagerFixture_ fixture;
    fixture.AddWorldView();
    FakeOverlay_* pOverlay = fixture.PushOverlay();

    pOverlay->Close();
    fixture.manager.ProcessInput();

    CHECK_FALSE(fixture.manager.HasOverlayView());
    CHECK(pOverlay->popCount == 1);
    CHECK(fixture.graphics.displayCount == 0);
}

TEST_CASE("The turn gate closes for an overlay and for an in-view modal", "[ui][manager]")
{
    ManagerFixture_ fixture;
    FakeWorldView_* pWorld = fixture.AddWorldView();
    CHECK(fixture.manager.CanAdvanceTurn());

    fixture.PushOverlay();
    CHECK_FALSE(fixture.manager.CanAdvanceTurn());

    fixture.manager.PopView();
    CHECK(fixture.manager.CanAdvanceTurn());

    pWorld->bBlocksTurn = true;
    CHECK_FALSE(fixture.manager.CanAdvanceTurn());
}

TEST_CASE("Camera input is driven from Update, not from Render", "[ui][manager]")
{
    // Edge scrolling used to run on the paint path, where a second render pass would apply it
    // twice.
    ManagerFixture_ fixture;
    FakeWorldView_* pWorld = fixture.AddWorldView();

    fixture.manager.Render();
    CHECK(pWorld->cameraUpdateCount == 0);
    CHECK(pWorld->renderCount == 1);

    fixture.manager.Update();
    CHECK(pWorld->cameraUpdateCount == 1);
    CHECK(pWorld->autoEndTurnCount == 1);

}

TEST_CASE("A queued auto end-turn survives an overlay instead of being dropped",
          "[ui][manager]")
{
    // ProcessPendingAutoEndTurn clears its flag before calling through, and Engine's own gate
    // then declines to advance — so calling it under an overlay silently loses the request.
    ManagerFixture_ fixture;
    FakeWorldView_* pWorld = fixture.AddWorldView();

    fixture.PushOverlay();
    fixture.manager.Update();
    CHECK(pWorld->autoEndTurnCount == 0);

    fixture.manager.PopView();
    fixture.manager.Update();
    CHECK(pWorld->autoEndTurnCount == 1);
}

TEST_CASE("Edge scrolling is disabled while an overlay covers the map", "[ui][manager]")
{
    ManagerFixture_ fixture;
    FakeWorldView_* pWorld = fixture.AddWorldView();

    fixture.manager.Update();
    CHECK(pWorld->bLastCameraEnabled);

    fixture.PushOverlay();
    fixture.manager.Update();
    CHECK_FALSE(pWorld->bLastCameraEnabled);
}

TEST_CASE("The last mouse position reaches the camera", "[ui][manager]")
{
    ManagerFixture_ fixture;
    FakeWorldView_* pWorld = fixture.AddWorldView();

    fixture.manager.Update();
    CHECK_FALSE(pWorld->lastMousePosition.has_value());

    fixture.queue.PushMouse({MouseButton_t::None, 42, 24, {}, false});
    fixture.manager.Update();
    REQUIRE(pWorld->lastMousePosition.has_value());
    CHECK(pWorld->lastMousePosition->x == 42);
    CHECK(pWorld->lastMousePosition->y == 24);
}

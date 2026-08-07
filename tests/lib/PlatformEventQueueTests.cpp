#include "input/Input.h"
#include "input/PlatformEventQueue.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace ac;

TEST_CASE("PlatformEventQueue hands events back in order, once", "[input][platform]")
{
    PlatformEventQueue queue;

    CHECK_FALSE(queue.PopKey().has_value());
    CHECK_FALSE(queue.PopMouse().has_value());

    queue.PushKey({Key_t::A, {}});
    queue.PushKey({Key_t::B, {}});

    auto first = queue.PopKey();
    REQUIRE(first.has_value());
    CHECK(first->key == Key_t::A);

    auto second = queue.PopKey();
    REQUIRE(second.has_value());
    CHECK(second->key == Key_t::B);

    CHECK_FALSE(queue.PopKey().has_value());
}

TEST_CASE("PlatformEventQueue carries modifiers with the key", "[input][platform]")
{
    PlatformEventQueue queue;
    queue.PushKey({Key_t::S, ModifierState_t{/*bCtrl=*/true, false, false}});

    const auto event = queue.PopKey();
    REQUIRE(event.has_value());
    CHECK(event->key == Key_t::S);
    CHECK(event->modifier.bCtrl);
    CHECK_FALSE(event->modifier.bAlt);
}

TEST_CASE("The last mouse position is empty until a mouse event arrives", "[input][platform]")
{
    PlatformEventQueue queue;
    CHECK_FALSE(queue.GetLastMousePosition().has_value());

    queue.PushMouse({MouseButton_t::Left, 12, 34, {}, true});

    const auto position = queue.GetLastMousePosition();
    REQUIRE(position.has_value());
    CHECK(position->x == 12);
    CHECK(position->y == 34);

    // Draining the event does not forget where the pointer is.
    REQUIRE(queue.PopMouse().has_value());
    REQUIRE(queue.GetLastMousePosition().has_value());
    CHECK(queue.GetLastMousePosition()->x == 12);

    queue.PushMouse({MouseButton_t::None, 99, 100, {}, false});
    CHECK(queue.GetLastMousePosition()->x == 99);
}

TEST_CASE("A close request is data the engine consumes, not a policy the backend applies",
          "[input][platform]")
{
    PlatformEventQueue queue;
    CHECK_FALSE(queue.TakeCloseRequest());

    queue.RequestClose();
    CHECK(queue.TakeCloseRequest());
    // Taking it clears it, so one click is one request.
    CHECK_FALSE(queue.TakeCloseRequest());
}

TEST_CASE("Input reads whatever filled the queue, whichever backend that was", "[input]")
{
    // The point of the seam: Input names no windowing library, so a queue filled by anything -
    // here, the test itself - is indistinguishable from one filled by a window.
    PlatformEventQueue queue;
    const std::unique_ptr<Input> pInput = CreateInput(queue);
    REQUIRE(pInput);

    CHECK_FALSE(pInput->PollKey().has_value());
    CHECK_FALSE(pInput->PollMouse().has_value());
    CHECK_FALSE(pInput->GetLastMousePosition().has_value());

    queue.PushKey({Key_t::Escape, ModifierState_t{false, false, /*bShift=*/true}});
    queue.PushMouse({MouseButton_t::Right, 7, 9, {}, true});

    const auto key = pInput->PollKey();
    REQUIRE(key.has_value());
    CHECK(key->key == Key_t::Escape);
    CHECK(key->modifier.bShift);

    const auto mouse = pInput->PollMouse();
    REQUIRE(mouse.has_value());
    CHECK(mouse->button == MouseButton_t::Right);
    CHECK(mouse->x == 7);

    REQUIRE(pInput->GetLastMousePosition().has_value());
    CHECK(pInput->GetLastMousePosition()->y == 9);

    // Drained: polling again is empty, and never blocks.
    CHECK_FALSE(pInput->PollKey().has_value());
    CHECK_FALSE(pInput->PollMouse().has_value());
}

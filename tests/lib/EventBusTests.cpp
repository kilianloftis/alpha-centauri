#include "lib/EventBus.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace ac;

namespace
{

GameEvent AnyEvent_()
{
    return GameEvent{EvTurnStarted{}};
}

} // namespace

TEST_CASE("EventBus delivers to every subscriber", "[lib][eventbus]")
{
    EventBus bus;
    int first = 0;
    int second = 0;
    bus.Subscribe([&](const GameEvent&) { ++first; });
    bus.Subscribe([&](const GameEvent&) { ++second; });

    bus.Publish(AnyEvent_());
    CHECK(first == 1);
    CHECK(second == 1);
}

TEST_CASE("Subscribing during dispatch does not invalidate the dispatch", "[lib][eventbus]")
{
    // Pins the contract only. It cannot prove the absence of the reallocation hazard, because
    // reading a reallocated vector may happen to work; the unsubscribe case below is the one
    // that fails deterministically without the fix.
    EventBus bus;
    int outer = 0;
    int added = 0;

    bus.Subscribe([&](const GameEvent&) {
        ++outer;
        if (outer == 1)
        {
            // Enough to force at least one reallocation of the handler vector.
            for (int i = 0; i < 64; ++i)
            {
                bus.Subscribe([&](const GameEvent&) { ++added; });
            }
        }
    });

    CHECK_NOTHROW(bus.Publish(AnyEvent_()));
    CHECK(outer == 1);
    // Handlers added mid-dispatch do not see the in-flight event...
    CHECK(added == 0);
    // ...but do see the next one.
    bus.Publish(AnyEvent_());
    CHECK(added == 64);
}

TEST_CASE("Unsubscribing during dispatch skips the removed handler", "[lib][eventbus]")
{
    EventBus bus;
    int firstCalls = 0;
    int secondCalls = 0;

    SubscriptionId secondId = 0;
    bus.Subscribe([&](const GameEvent&) {
        ++firstCalls;
        bus.Unsubscribe(secondId);
    });
    secondId = bus.Subscribe([&](const GameEvent&) { ++secondCalls; });

    CHECK_NOTHROW(bus.Publish(AnyEvent_()));
    CHECK(firstCalls == 1);
    // Removed before it was reached, so it must not be invoked from the snapshot either.
    CHECK(secondCalls == 0);
}

TEST_CASE("A handler that unsubscribes itself still completes", "[lib][eventbus]")
{
    EventBus bus;
    int calls = 0;
    SubscriptionId id = 0;
    id = bus.Subscribe([&](const GameEvent&) {
        ++calls;
        bus.Unsubscribe(id);
    });

    CHECK_NOTHROW(bus.Publish(AnyEvent_()));
    CHECK(calls == 1);

    bus.Publish(AnyEvent_());
    CHECK(calls == 1);
}

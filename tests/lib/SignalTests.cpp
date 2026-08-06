#include "lib/Signal.h"

#include <catch2/catch_test_macros.hpp>
#include <utility>
#include <vector>

using ac::Signal;
using ac::SlotId_t;

TEST_CASE("Signal Connect/Emit delivers arguments", "[signal]")
{
    Signal<int> sig;
    int seen = 0;
    sig.Connect([&](int v) { seen = v; });
    sig.Emit(42);
    REQUIRE(seen == 42);
}

TEST_CASE("Signal Disconnect stops further delivery", "[signal]")
{
    Signal<> sig;
    int count = 0;
    const SlotId_t id = sig.Connect([&]() { ++count; });
    sig.Emit();
    sig.Disconnect(id);
    sig.Emit();
    REQUIRE(count == 1);
}

TEST_CASE("Signal ScopedConnection disconnects on destruction", "[signal]")
{
    Signal<> sig;
    int count = 0;
    {
        auto conn = sig.ConnectScoped([&]() { ++count; });
        sig.Emit();
        REQUIRE(count == 1);
        REQUIRE(conn.IsConnected());
    }
    sig.Emit();
    REQUIRE(count == 1);
}

TEST_CASE("Signal ScopedConnection disconnects on move-assignment", "[signal]")
{
    Signal<> sig;
    int a = 0;
    int b = 0;
    auto conn = sig.ConnectScoped([&]() { ++a; });
    conn = sig.ConnectScoped([&]() { ++b; });
    sig.Emit();
    REQUIRE(a == 0);
    REQUIRE(b == 1);
}

TEST_CASE("Signal Emit is reentrancy-safe for Disconnect", "[signal]")
{
    Signal<> sig;
    int a = 0;
    int b = 0;
    SlotId_t idB = 0;

    sig.Connect([&]() {
        ++a;
        sig.Disconnect(idB);
    });
    idB = sig.Connect([&]() { ++b; });

    sig.Emit();
    REQUIRE(a == 1);
    REQUIRE(b == 0);

    sig.Emit();
    REQUIRE(a == 2);
    REQUIRE(b == 0);
}

TEST_CASE("Signal Emit is reentrancy-safe for Connect", "[signal]")
{
    Signal<> sig;
    std::vector<int> order;

    sig.Connect([&]() {
        order.push_back(1);
        sig.Connect([&]() { order.push_back(3); });
    });
    sig.Connect([&]() { order.push_back(2); });

    sig.Emit();
    REQUIRE(order == std::vector<int>{1, 2});

    order.clear();
    sig.Emit();
    REQUIRE(order == std::vector<int>{1, 2, 3});
}

TEST_CASE("Signal self-disconnect during Emit does not call later invocations", "[signal]")
{
    Signal<> sig;
    int count = 0;
    SlotId_t id = 0;
    id = sig.Connect([&]() {
        ++count;
        sig.Disconnect(id);
    });

    sig.Emit();
    sig.Emit();
    REQUIRE(count == 1);
}

TEST_CASE("Signal ScopedConnection outliving its Signal is inert, not a dangling write",
          "[signal][lifetime]")
{
    // The observer-outlives-subject case: a UI view connects to BaseManager::OnDestroyed, the
    // base emits and is destroyed, and the view is popped (and destructed) only afterwards.
    // ~ScopedConnection must not reach into the freed Signal to unregister itself.
    Signal<>::ScopedConnection conn;
    bool bFired = false;
    {
        Signal<> sig;
        conn = sig.ConnectScoped([&]() { bFired = true; });
        REQUIRE(conn.IsConnected());
        sig.Emit();
    }
    CHECK(bFired);
    // The token expired with the Signal, so the connection reports itself dead...
    CHECK_FALSE(conn.IsConnected());
    // ...and both explicit Disconnect and ~ScopedConnection (at scope exit) are no-ops.
    CHECK_NOTHROW(conn.Disconnect());
}

TEST_CASE("Signal ScopedConnection moved out of a dead Signal stays inert", "[signal][lifetime]")
{
    // Moving a connection must carry the alive token with it, or the move target would think
    // it still owns a live registration in a Signal that is already gone.
    Signal<>::ScopedConnection moved;
    {
        Signal<>::ScopedConnection conn;
        {
            Signal<> sig;
            conn = sig.ConnectScoped([]() {});
        }
        moved = std::move(conn);
    }
    CHECK_FALSE(moved.IsConnected());
    CHECK_NOTHROW(moved.Disconnect());
}

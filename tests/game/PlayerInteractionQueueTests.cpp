#include "game/PlayerInteraction.h"
#include "game/PlayerInteractionQueue.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

using namespace ac;

TEST_CASE("PlayerInteractionQueue is FIFO and reports pending for audience",
          "[PlayerInteraction][queue]")
{
    PlayerInteractionQueue queue;
    CHECK(queue.Empty());
    CHECK_FALSE(queue.HasPendingFor(1));

    QueuedInteraction_t first;
    first.payload = NoticeInteraction_t{PauseOnEventId_t::NewFacilityBuilt, "A", "body",
                                       std::nullopt};
    first.audience = 1;
    queue.Enqueue(std::move(first));

    QueuedInteraction_t second;
    second.payload = NoticeInteraction_t{PauseOnEventId_t::CombatUnitBuilt, "B", "body",
                                        std::nullopt};
    second.audience = 1;
    queue.Enqueue(std::move(second));

    REQUIRE(queue.Front());
    CHECK(std::get<NoticeInteraction_t>(queue.Front()->payload).title == "A");
    CHECK(queue.HasPendingFor(1));
    CHECK_FALSE(queue.HasPendingFor(2));

    queue.CompleteFront();
    REQUIRE(queue.Front());
    CHECK(std::get<NoticeInteraction_t>(queue.Front()->payload).title == "B");
    CHECK(queue.HasPendingFor(1));

    queue.CompleteFront();
    CHECK(queue.Empty());
    CHECK_FALSE(queue.HasPendingFor(1));
}

TEST_CASE("Completing nothing is a programmer error, not a silent no-op",
          "[PlayerInteraction][queue]")
{
    // A no-op would let a double-complete drop the *next* player's prompt instead, which
    // strands turn processing on a stage that is still yielding for it.
    PlayerInteractionQueue queue;
    CHECK_THROWS_AS(queue.CompleteFront(), std::runtime_error);
}

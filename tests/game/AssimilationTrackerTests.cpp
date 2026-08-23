#include "game/faction/base/population/AssimilationTracker.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

namespace
{

constexpr FactionId_t k_A = 1;
constexpr FactionId_t k_B = 2;
constexpr FactionId_t k_C = 3;
constexpr int k_Peak = 5;
constexpr int k_Decay = 10;

void Advance_(AssimilationTracker& rTracker, int turns)
{
    for (int i = 0; i < turns; ++i)
    {
        rTracker.Advance();
    }
}

} // namespace

TEST_CASE("A third-party capture keeps the original owner's reclaim claim",
          "[population][assimilation]")
{
    AssimilationTracker tracker;
    tracker.NotifyCaptured(k_A, k_B, k_Peak, k_Decay);
    Advance_(tracker, 12);
    CHECK(tracker.ClaimFor(k_A).turnsElapsed == 12);

    tracker.NotifyCaptured(k_B, k_C, k_Peak, k_Decay);
    CHECK(tracker.OccupierWindow().peakDrones == 5);
    CHECK(tracker.OccupierWindow().durationTurns == 50);
    CHECK(tracker.ClaimFor(k_A).IsActive());
    CHECK(tracker.ClaimFor(k_A).turnsElapsed == 12);
    CHECK(tracker.ClaimFor(k_B).IsActive());
    CHECK(tracker.ClaimFor(k_B).turnsElapsed == 0);

    tracker.NotifyCaptured(k_C, k_A, k_Peak, k_Decay);
    CHECK(tracker.IsAssimilating());
    CHECK(tracker.OccupierWindow().peakDrones == 1);
    CHECK(tracker.OccupierWindow().durationTurns == 12);
    CHECK(tracker.OccupierWindow().formerOwner == k_C);
    CHECK_FALSE(tracker.ClaimFor(k_A).IsActive());
}

TEST_CASE("A delayed original-owner recapture uses total time away, not the last occupier's clock",
          "[population][assimilation]")
{
    AssimilationTracker tracker;
    tracker.NotifyCaptured(k_A, k_B, k_Peak, k_Decay);
    Advance_(tracker, 12);
    tracker.NotifyCaptured(k_B, k_C, k_Peak, k_Decay);
    Advance_(tracker, 5);

    tracker.NotifyCaptured(k_C, k_A, k_Peak, k_Decay);
    CHECK(tracker.OccupierWindow().peakDrones == 1);
    CHECK(tracker.OccupierWindow().durationTurns == 17);
}

TEST_CASE("An expired original-owner claim is a full conquest", "[population][assimilation]")
{
    AssimilationTracker tracker;
    tracker.NotifyCaptured(k_A, k_B, k_Peak, k_Decay);
    Advance_(tracker, 12);
    tracker.NotifyCaptured(k_B, k_C, k_Peak, k_Decay);
    Advance_(tracker, 40); // A's claim started at B's capture: 12+40 = 52 > 50
    CHECK_FALSE(tracker.ClaimFor(k_A).IsActive());

    tracker.NotifyCaptured(k_C, k_A, k_Peak, k_Decay);
    CHECK(tracker.OccupierWindow().peakDrones == 5);
    CHECK(tracker.OccupierWindow().durationTurns == 50);
}

TEST_CASE("A to B to C to B to A reverses each reclaim from that faction's own claim",
          "[population][assimilation]")
{
    // A loses → B holds 12 → C holds 10 → B retakes → A retakes.
    // B's reclaim uses B's 10-turn claim (1 drone / 10 turns), not a fresh 5/50.
    // A's reclaim uses A's continuous time away (12+10+5 = 27 → 2 drones / 27 turns).
    AssimilationTracker tracker;
    tracker.NotifyCaptured(k_A, k_B, k_Peak, k_Decay);
    Advance_(tracker, 12);

    tracker.NotifyCaptured(k_B, k_C, k_Peak, k_Decay);
    Advance_(tracker, 10);
    CHECK(tracker.ClaimFor(k_A).turnsElapsed == 22);
    CHECK(tracker.ClaimFor(k_B).turnsElapsed == 10);

    tracker.NotifyCaptured(k_C, k_B, k_Peak, k_Decay);
    CHECK(tracker.OccupierWindow().peakDrones == 1);
    CHECK(tracker.OccupierWindow().durationTurns == 10);
    CHECK(tracker.OccupierWindow().formerOwner == k_C);
    CHECK_FALSE(tracker.ClaimFor(k_B).IsActive());
    CHECK(tracker.ClaimFor(k_A).IsActive());
    CHECK(tracker.ClaimFor(k_A).turnsElapsed == 22);

    Advance_(tracker, 5);
    CHECK(tracker.ClaimFor(k_A).turnsElapsed == 27);

    tracker.NotifyCaptured(k_B, k_A, k_Peak, k_Decay);
    CHECK(tracker.OccupierWindow().peakDrones == 2);
    CHECK(tracker.OccupierWindow().durationTurns == 27);
    CHECK(tracker.OccupierWindow().formerOwner == k_B);
    CHECK_FALSE(tracker.ClaimFor(k_A).IsActive());
}

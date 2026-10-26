#include "game/population/AssimilationTracker.h"
#include "game/population/PopulationRules.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

namespace
{

constexpr FactionId_t k_A = 1;
constexpr FactionId_t k_B = 2;
constexpr FactionId_t k_C = 3;
constexpr int k_Peak = 5;
constexpr int k_Decay = 10;
constexpr int k_InciteTurns = 10;
constexpr double k_InciteCapFactor = 2.5;

int TurnsLeft_(const AssimilationState& rWindow)
{
    return rWindow.durationTurns - rWindow.turnsElapsed;
}

void Advance_(AssimilationTracker& rTracker, int turns, FactionId_t currentOwner)
{
    for (int i = 0; i < turns; ++i)
    {
        rTracker.Advance(currentOwner);
    }
}

} // namespace

TEST_CASE("A third-party capture keeps the original owner's reclaim claim",
          "[population][assimilation]")
{
    AssimilationTracker tracker;
    tracker.NotifyCaptured(k_A, k_B, k_Peak, k_Decay);
    Advance_(tracker, 12, k_B);
    CHECK(tracker.ClaimFor(k_A).turnsElapsed == 12);

    tracker.NotifyCaptured(k_B, k_C, k_Peak, k_Decay);
    CHECK(tracker.OccupierWindow().peakDrones == 5);
    CHECK(tracker.OccupierWindow().durationTurns == 50);
    CHECK(tracker.ClaimFor(k_A).IsActive());
    CHECK(tracker.ClaimFor(k_A).turnsElapsed == 12);
    CHECK(tracker.ClaimFor(k_B).IsActive());
    CHECK(tracker.ClaimFor(k_B).turnsElapsed == 0);

    tracker.NotifyCaptured(k_C, k_A, k_Peak, k_Decay);
    CHECK(tracker.OccupierWindow().IsActive());
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
    Advance_(tracker, 12, k_B);
    tracker.NotifyCaptured(k_B, k_C, k_Peak, k_Decay);
    Advance_(tracker, 5, k_C);

    tracker.NotifyCaptured(k_C, k_A, k_Peak, k_Decay);
    CHECK(tracker.OccupierWindow().peakDrones == 1);
    CHECK(tracker.OccupierWindow().durationTurns == 17);
}

TEST_CASE("An expired original-owner claim is a full conquest", "[population][assimilation]")
{
    AssimilationTracker tracker;
    tracker.NotifyCaptured(k_A, k_B, k_Peak, k_Decay);
    Advance_(tracker, 12, k_B);
    tracker.NotifyCaptured(k_B, k_C, k_Peak, k_Decay);
    Advance_(tracker, 40, k_C); // A's claim started at B's capture: 12+40 = 52 > 50
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
    Advance_(tracker, 12, k_B);

    tracker.NotifyCaptured(k_B, k_C, k_Peak, k_Decay);
    Advance_(tracker, 10, k_C);
    CHECK(tracker.ClaimFor(k_A).turnsElapsed == 22);
    CHECK(tracker.ClaimFor(k_B).turnsElapsed == 10);

    tracker.NotifyCaptured(k_C, k_B, k_Peak, k_Decay);
    CHECK(tracker.OccupierWindow().peakDrones == 1);
    CHECK(tracker.OccupierWindow().durationTurns == 10);
    CHECK(tracker.OccupierWindow().formerOwner == k_C);
    CHECK_FALSE(tracker.ClaimFor(k_B).IsActive());
    CHECK(tracker.ClaimFor(k_A).IsActive());
    CHECK(tracker.ClaimFor(k_A).turnsElapsed == 22);

    Advance_(tracker, 5, k_B);
    CHECK(tracker.ClaimFor(k_A).turnsElapsed == 27);

    tracker.NotifyCaptured(k_B, k_A, k_Peak, k_Decay);
    CHECK(tracker.OccupierWindow().peakDrones == 2);
    CHECK(tracker.OccupierWindow().durationTurns == 27);
    CHECK(tracker.OccupierWindow().formerOwner == k_B);
    CHECK_FALSE(tracker.ClaimFor(k_A).IsActive());
}

TEST_CASE("AssimilationOnCapture starts a fresh window and reverses a claim on recapture",
          "[population][assimilation]")
{
    AssimilationTracker tracker;
    const AssimilationOnCapture_t first =
        AssimilationOnCapture(tracker, k_A, k_B, k_Peak, k_Decay);
    CHECK_FALSE(first.bReclaims);
    CHECK(first.occupier.peakDrones == 5);
    CHECK(first.occupier.durationTurns == 50);
    REQUIRE(first.previousOwnerClaim.has_value());
    CHECK(first.previousOwnerClaim->formerOwner == k_A);

    tracker.NotifyCaptured(k_A, k_B, k_Peak, k_Decay);
    Advance_(tracker, 20, k_B);

    // Twenty turns away at one drone per ten turns reverses into two drones over twenty turns.
    const AssimilationOnCapture_t back = AssimilationOnCapture(tracker, k_B, k_A, k_Peak, k_Decay);
    CHECK(back.bReclaims);
    CHECK(back.occupier.peakDrones == 2);
    CHECK(back.occupier.durationTurns == 20);
    CHECK(back.previousOwnerClaim.has_value());
}

TEST_CASE("Incite on a calm base opens a window with the added turns left",
          "[population][assimilation][incite]")
{
    AssimilationTracker tracker;
    tracker.NotifyIncited(/*baseSize=*/4, k_Decay, k_InciteTurns, k_InciteCapFactor);
    const AssimilationState& rOccupier = tracker.OccupierWindow();
    CHECK(rOccupier.IsActive());
    CHECK(rOccupier.formerOwner == -1);
    CHECK(TurnsLeft_(rOccupier) == 10);
    CHECK(rOccupier.peakDrones == 1);
    CHECK_FALSE(tracker.HasClaim(k_A));
    CHECK_FALSE(tracker.HasClaim(k_B));
}

TEST_CASE("Incite on size 4 does not grow past ten turns left",
          "[population][assimilation][incite]")
{
    AssimilationTracker tracker;
    tracker.NotifyIncited(/*baseSize=*/4, k_Decay, k_InciteTurns, k_InciteCapFactor);
    tracker.NotifyIncited(/*baseSize=*/4, k_Decay, k_InciteTurns, k_InciteCapFactor);
    CHECK(TurnsLeft_(tracker.OccupierWindow()) == 10);
}

TEST_CASE("Incite does not extend a window already at the size cap",
          "[population][assimilation][incite]")
{
    AssimilationTracker tracker;
    tracker.NotifyCaptured(k_A, k_B, k_Peak, k_Decay);
    tracker.NotifyIncited(/*baseSize=*/4, k_Decay, k_InciteTurns, k_InciteCapFactor);
    const AssimilationState& rOccupier = tracker.OccupierWindow();
    CHECK(TurnsLeft_(rOccupier) == 50);
    CHECK(rOccupier.peakDrones == k_Peak);
    CHECK(rOccupier.formerOwner == k_A);
}

TEST_CASE("Incite on a large base extends past the capture window",
          "[population][assimilation][incite]")
{
    // SMAC's cap is 10 * 24 / 4 = 60 turns, so a sixth drone appears.
    AssimilationTracker tracker;
    tracker.NotifyCaptured(k_A, k_B, k_Peak, k_Decay);
    tracker.NotifyIncited(/*baseSize=*/24, k_Decay, k_InciteTurns, k_InciteCapFactor);
    const AssimilationState& rOccupier = tracker.OccupierWindow();
    CHECK(TurnsLeft_(rOccupier) == 60);
    CHECK(rOccupier.peakDrones == 6);
    CHECK(rOccupier.formerOwner == k_A);
}

TEST_CASE("Incite on a reclaim window keeps that window's decay rate",
          "[population][assimilation][incite]")
{
    AssimilationState reclaim;
    reclaim.formerOwner = k_A;
    reclaim.peakDrones = 2;
    reclaim.durationTurns = 27;
    reclaim.turnsElapsed = 7;

    const AssimilationState occupier =
        AssimilationOnIncite(reclaim, /*baseSize=*/12, k_Decay, k_InciteTurns, k_InciteCapFactor);
    CHECK(TurnsLeft_(occupier) == 30);
    // 27 / 2 decays one drone per 13 turns: 30 turns left is ceil(30 / 13) = 3 drones.
    CHECK(occupier.peakDrones == 3);
    CHECK(occupier.durationTurns == 39);
    CHECK(occupier.formerOwner == k_A);
}

TEST_CASE("A never-captured incite window expires after five double-ticks",
          "[population][assimilation][incite]")
{
    AssimilationTracker tracker;
    tracker.NotifyIncited(/*baseSize=*/4, k_Decay, k_InciteTurns, k_InciteCapFactor);
    Advance_(tracker, 4, k_B);
    CHECK(tracker.OccupierWindow().IsActive());
    Advance_(tracker, 1, k_B);
    CHECK_FALSE(tracker.OccupierWindow().IsActive());
}

TEST_CASE("OccupierDecayTicks doubles only while the former owner holds the base",
          "[population][assimilation]")
{
    AssimilationState captured;
    captured.formerOwner = k_A;
    CHECK(OccupierDecayTicks(captured, k_B) == 1);
    CHECK(OccupierDecayTicks(captured, k_A) == 2);
    CHECK(OccupierDecayTicks(AssimilationState{}, k_B) == 2);
}

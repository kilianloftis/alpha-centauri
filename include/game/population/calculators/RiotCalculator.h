#pragma once

#include "game/population/calculators/MoodLatch.h"

namespace ac
{

// Inputs for the natural riot condition: the seated pops' riot weights against the base's
// threshold. Drones weigh +1 (Super Drones too — a super absorbs two drones of *pressure*, but
// riots like any single citizen), talents −1, and everything else 0.
struct RiotConditionInputs_t
{
    int riotSum = 0;
    int threshold = 0;
};

// Tracks drone riot state for a base population. See MoodLatch for the Forecast / Commit
// lifecycle this shares with GoldenAgeCalculator.
//
// Two independent sources keep a riot alive once committed:
//   - the natural condition (riotSum >= riot_threshold), recomputed on every Commit
//   - a forced riot (probe Incite Drone Riots), which lasts a fixed number of Mood commits
//     and which the natural condition cannot sustain, since the action does not change
//     composition
class RiotCalculator : public MoodLatch
{
public:
    RiotCalculator(Signal<>& rWillRiot, Signal<>& rIsRioting, Signal<>& rRiotEnded);
    ~RiotCalculator() = default;

    // Call after a pop is added, so growth into a riot warns immediately rather than waiting
    // for the next Population stage.
    void NotifyPopGrown(const RiotConditionInputs_t& rInputs);

    void Forecast(const RiotConditionInputs_t& rInputs);

    // Additionally ages the forced timer by one pass and maintains the consecutive count.
    void Commit(const RiotConditionInputs_t& rInputs);

    // Force a riot for the next `turns` Mood commits. Extends but never shortens an existing
    // forced timer. Does not activate until Commit — sets pending so the player is still
    // warned if Forecast already ran this turn.
    void ForceRiot(int turns);

    // Consecutive Mood commits while active; selects the active riot tier. 0 when calm.
    int GetConsecutiveTurns() const { return m_consecutiveTurns; }
    int GetForcedTurnsRemaining() const { return m_forcedTurnsRemaining; }

    // Save/load without emitting signals.
    void RestoreState(bool bActive, bool bPending, int forcedTurnsRemaining,
                      int consecutiveTurns);

private:
    static bool NaturalCondition_(const RiotConditionInputs_t& rInputs);

    int m_forcedTurnsRemaining = 0;
    int m_consecutiveTurns = 0;
};

} // namespace ac

#pragma once

#include "game/population/MoodLatch.h"

namespace ac
{

// Tracks drone riot state for a base population. See MoodLatch for the Forecast / Commit
// lifecycle this shares with GoldenAgeLatch.
//
// The condition comes from RiotConditionMet, which counts a forced riot (probe Incite Drone
// Riots) as well as the composition. This latch ages the forced timer and counts the turns.
class RiotLatch : public MoodLatch
{
public:
    RiotLatch(Signal<>& rWillRiot, Signal<>& rIsRioting, Signal<>& rRiotEnded);
    ~RiotLatch() = default;

    // Call after a pop is added, so growth into a riot warns immediately rather than waiting
    // for the next Population stage.
    void NotifyPopGrown(bool bRiotCondition);

    // Additionally ages the forced timer by one pass and maintains the consecutive count.
    void Commit(bool bRiotCondition);

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
    int m_forcedTurnsRemaining = 0;
    int m_consecutiveTurns = 0;
};

} // namespace ac

#pragma once

#include "game/population/MoodLatch.h"

namespace ac
{

// Riot state for a base (see MoodLatch). RiotConditionMet counts forced riots; this ages the
// forced timer and counts turns.
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

    // Forces a riot for the next `turns` commits, never shortening one. Pending until Commit.
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

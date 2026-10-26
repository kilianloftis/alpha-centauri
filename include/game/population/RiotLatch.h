#pragma once

#include "game/population/MoodLatch.h"

namespace ac
{

// Riot state for a base (see MoodLatch). Ages consecutive commits for riot_tiers.
class RiotLatch : public MoodLatch
{
public:
    RiotLatch(Signal<>& rWillRiot, Signal<>& rIsRioting, Signal<>& rRiotEnded);
    ~RiotLatch() = default;

    // Call after a pop is added, so growth into a riot warns immediately rather than waiting
    // for the next Population stage.
    void NotifyPopGrown(bool bRiotCondition);

    void Commit(bool bRiotCondition);

    // Consecutive Mood commits while active; selects the active riot tier. 0 when calm.
    int GetConsecutiveTurns() const { return m_consecutiveTurns; }

    // Save/load without emitting signals.
    void RestoreState(bool bActive, bool bPending, int consecutiveTurns);

private:
    int m_consecutiveTurns = 0;
};

} // namespace ac

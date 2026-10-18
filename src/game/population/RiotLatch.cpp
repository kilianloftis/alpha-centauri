#include "game/population/RiotLatch.h"

#include "game/population/PopulationRules.h"

#include <algorithm>

namespace ac
{

RiotLatch::RiotLatch(Signal<>& rWillRiot, Signal<>& rIsRioting, Signal<>& rRiotEnded)
    : MoodLatch(rWillRiot, rIsRioting, rRiotEnded)
{
}

void RiotLatch::NotifyPopGrown(bool bRiotCondition)
{
    if (bRiotCondition)
    {
        SetPendingAndWarn_();
    }
}

void RiotLatch::Commit(bool bRiotCondition)
{
    // The condition was read before this pass consumes a forced turn, so ForceRiot(1)
    // survives exactly the next Commit.
    if (m_forcedTurnsRemaining > 0)
    {
        --m_forcedTurnsRemaining;
    }

    if (Commit_(bRiotCondition) == Transition_t::Ended)
    {
        m_consecutiveTurns = 0;
        return;
    }

    if (IsActive())
    {
        ++m_consecutiveTurns;
    }
}

void RiotLatch::ForceRiot(int turns)
{
    m_forcedTurnsRemaining = ForcedRiotTurns(m_forcedTurnsRemaining, turns);
    if (m_forcedTurnsRemaining > 0)
    {
        SetPendingAndWarn_();
    }
}

void RiotLatch::RestoreState(bool bActive, bool bPending, int forcedTurnsRemaining,
                                  int consecutiveTurns)
{
    Restore_(bActive, bPending);
    m_forcedTurnsRemaining = std::max(0, forcedTurnsRemaining);
    m_consecutiveTurns = bActive ? std::max(0, consecutiveTurns) : 0;
}

} // namespace ac

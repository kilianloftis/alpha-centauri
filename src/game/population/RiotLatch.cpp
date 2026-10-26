#include "game/population/RiotLatch.h"

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
    if (MoodLatch::Commit(bRiotCondition) == Transition_t::Ended)
    {
        m_consecutiveTurns = 0;
        return;
    }

    if (IsActive())
    {
        ++m_consecutiveTurns;
    }
}

void RiotLatch::RestoreState(bool bActive, bool bPending, int consecutiveTurns)
{
    MoodLatch::RestoreState(bActive, bPending);
    m_consecutiveTurns = bActive ? std::max(0, consecutiveTurns) : 0;
}

} // namespace ac

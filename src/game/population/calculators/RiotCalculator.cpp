#include "game/population/calculators/RiotCalculator.h"

#include <algorithm>

namespace ac
{

RiotCalculator::RiotCalculator(Signal<>& rWillRiot, Signal<>& rIsRioting, Signal<>& rRiotEnded)
    : MoodLatch(rWillRiot, rIsRioting, rRiotEnded)
{
}

bool RiotCalculator::NaturalCondition_(const RiotConditionInputs_t& rInputs)
{
    return rInputs.riotSum >= rInputs.threshold;
}

void RiotCalculator::NotifyPopGrown(const RiotConditionInputs_t& rInputs)
{
    if (NaturalCondition_(rInputs))
    {
        SetPendingAndWarn_();
    }
}

void RiotCalculator::Forecast(const RiotConditionInputs_t& rInputs)
{
    // A forced riot holds pending through a calm composition: the probe bought turns the
    // natural condition was never going to supply.
    Forecast_(NaturalCondition_(rInputs), /*bHoldPending=*/m_forcedTurnsRemaining > 0);
}

void RiotCalculator::Commit(const RiotConditionInputs_t& rInputs)
{
    // This Mood pass is one of the turns the forced riot was bought for, so it counts before
    // it is consumed: ForceRiot(1) survives exactly the next Commit.
    const bool bForced = m_forcedTurnsRemaining > 0;
    if (bForced)
    {
        --m_forcedTurnsRemaining;
    }

    if (Commit_(NaturalCondition_(rInputs) || bForced) == Transition_t::Ended)
    {
        m_consecutiveTurns = 0;
        return;
    }

    if (IsActive())
    {
        ++m_consecutiveTurns;
    }
}

void RiotCalculator::ForceRiot(int turns)
{
    m_forcedTurnsRemaining = std::max(m_forcedTurnsRemaining, std::max(0, turns));
    if (m_forcedTurnsRemaining > 0)
    {
        SetPendingAndWarn_();
    }
}

void RiotCalculator::RestoreState(bool bActive, bool bPending, int forcedTurnsRemaining,
                                  int consecutiveTurns)
{
    Restore_(bActive, bPending);
    m_forcedTurnsRemaining = std::max(0, forcedTurnsRemaining);
    m_consecutiveTurns = bActive ? std::max(0, consecutiveTurns) : 0;
}

} // namespace ac

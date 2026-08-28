#include "game/population/calculators/RiotCalculator.h"

#include <algorithm>

namespace ac
{

RiotCalculator::RiotCalculator(Signal<>& rWillRiot, Signal<>& rIsRioting, Signal<>& rRiotEnded)
    : m_rWillRiot(rWillRiot)
    , m_rIsRioting(rIsRioting)
    , m_rRiotEnded(rRiotEnded)
{
}

void RiotCalculator::NotifyPopGrown(const RiotConditionInputs_t& rInputs)
{
    if (!m_bRioting && NaturalCondition_(rInputs))
    {
        m_rWillRiot.Emit();
    }
}

void RiotCalculator::Update(const RiotConditionInputs_t& rInputs)
{
    // This pass is one of the turns the forced riot was bought for, so it counts before it is
    // consumed: ForceRiot(1) survives exactly the next end of turn.
    const bool bForced = m_forcedTurnsRemaining > 0;
    if (bForced)
    {
        --m_forcedTurnsRemaining;
    }

    const bool bRioting = NaturalCondition_(rInputs) || bForced;
    if (bRioting)
    {
        m_bRioting = true;
        m_rIsRioting.Emit();
    }
    else if (m_bRioting)
    {
        m_bRioting = false;
        m_rRiotEnded.Emit();
    }
}

void RiotCalculator::ForceRiot(int turns)
{
    m_forcedTurnsRemaining = std::max(m_forcedTurnsRemaining, std::max(0, turns));
    if (m_forcedTurnsRemaining > 0 && !m_bRioting)
    {
        m_bRioting = true;
        m_rIsRioting.Emit();
    }
}

bool RiotCalculator::IsRioting() const
{
    return m_bRioting;
}

bool RiotCalculator::NaturalCondition_(const RiotConditionInputs_t& rInputs)
{
    return rInputs.riotSum >= rInputs.threshold;
}

} // namespace ac

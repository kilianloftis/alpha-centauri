#include "game/faction/base/population/calculators/RiotCalculator.h"

namespace ac
{

RiotCalculator::RiotCalculator(Signal<>& rWillRiot, Signal<>& rIsRioting, Signal<>& rRiotEnded)
    : m_rWillRiot(rWillRiot)
    , m_rIsRioting(rIsRioting)
    , m_rRiotEnded(rRiotEnded)
{
}

void RiotCalculator::NotifyPopGrown(const RiotConditionInputs& inputs)
{
    if (!m_bRioting && ComputeCondition_(inputs))
    {
        m_rWillRiot.emit();
    }
}

void RiotCalculator::Update(const RiotConditionInputs& inputs)
{
    if (ComputeCondition_(inputs))
    {
        m_bRioting = true;
        m_rIsRioting.emit();
    }
    else if (m_bRioting)
    {
        m_bRioting = false;
        m_rRiotEnded.emit();
    }
}

bool RiotCalculator::IsRioting() const
{
    return m_bRioting;
}

bool RiotCalculator::ComputeCondition_(const RiotConditionInputs& inputs)
{
    int threshold = (inputs.targetTalents >= 0) ? inputs.targetTalents : inputs.talentCount;
    return inputs.droneCount > threshold;
}

} // namespace ac

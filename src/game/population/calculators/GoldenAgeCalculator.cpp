#include "game/population/calculators/GoldenAgeCalculator.h"

namespace ac
{

GoldenAgeCalculator::GoldenAgeCalculator(Signal<>& rGoldenAgeStarted, Signal<>& rGoldenAgeEnded)
    : m_rGoldenAgeStarted(rGoldenAgeStarted)
    , m_rGoldenAgeEnded(rGoldenAgeEnded)
{
}

bool GoldenAgeCalculator::EvaluateCondition_(const Inputs_t& inputs)
{
    if (inputs.droneCount > 0)
    {
        return false;
    }
    return inputs.talentCount >= (inputs.workerCount + inputs.specialistCount);
}

void GoldenAgeCalculator::Update(const Inputs_t& inputs)
{
    const bool bCondition = EvaluateCondition_(inputs);
    if (bCondition && !m_bInGoldenAge)
    {
        m_bInGoldenAge = true;
        m_rGoldenAgeStarted.Emit();
    }
    else if (!bCondition && m_bInGoldenAge)
    {
        m_bInGoldenAge = false;
        m_rGoldenAgeEnded.Emit();
    }
}

bool GoldenAgeCalculator::IsInGoldenAge() const
{
    return m_bInGoldenAge;
}

} // namespace ac

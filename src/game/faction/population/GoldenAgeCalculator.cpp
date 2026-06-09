#include "game/faction/population/GoldenAgeCalculator.h"

namespace ac
{

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
        golden_age_started.emit();
    }
    else if (!bCondition && m_bInGoldenAge)
    {
        m_bInGoldenAge = false;
        golden_age_ended.emit();
    }
}

bool GoldenAgeCalculator::IsInGoldenAge() const
{
    return m_bInGoldenAge;
}

} // namespace ac

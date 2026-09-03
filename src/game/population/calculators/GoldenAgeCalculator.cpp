#include "game/population/calculators/GoldenAgeCalculator.h"

namespace ac
{

GoldenAgeCalculator::GoldenAgeCalculator(Signal<>& rWillGoldenAge,
                                         Signal<>& rGoldenAgeStarted,
                                         Signal<>& rGoldenAgeEnded)
    : MoodLatch(rWillGoldenAge, rGoldenAgeStarted, rGoldenAgeEnded)
{
}

bool GoldenAgeCalculator::EvaluateCondition_(const Inputs_t& rInputs)
{
    if (rInputs.droneCount > 0)
    {
        return false;
    }
    return rInputs.goldenAgeSum >= rInputs.threshold;
}

void GoldenAgeCalculator::Forecast(const Inputs_t& rInputs)
{
    Forecast_(EvaluateCondition_(rInputs), /*bHoldPending=*/false);
}

void GoldenAgeCalculator::Commit(const Inputs_t& rInputs)
{
    Commit_(EvaluateCondition_(rInputs));
}

void GoldenAgeCalculator::RestoreState(bool bActive, bool bPending)
{
    Restore_(bActive, bPending);
}

} // namespace ac

#include "game/population/GoldenAgeLatch.h"

namespace ac
{

GoldenAgeLatch::GoldenAgeLatch(Signal<>& rWillGoldenAge,
                                         Signal<>& rGoldenAgeStarted,
                                         Signal<>& rGoldenAgeEnded)
    : MoodLatch(rWillGoldenAge, rGoldenAgeStarted, rGoldenAgeEnded)
{
}

bool GoldenAgeLatch::EvaluateCondition_(const Inputs_t& rInputs)
{
    if (rInputs.droneCount > 0)
    {
        return false;
    }
    return rInputs.goldenAgeSum >= rInputs.threshold;
}

void GoldenAgeLatch::Forecast(const Inputs_t& rInputs)
{
    Forecast_(EvaluateCondition_(rInputs), /*bHoldPending=*/false);
}

void GoldenAgeLatch::Commit(const Inputs_t& rInputs)
{
    Commit_(EvaluateCondition_(rInputs));
}

void GoldenAgeLatch::RestoreState(bool bActive, bool bPending)
{
    Restore_(bActive, bPending);
}

} // namespace ac

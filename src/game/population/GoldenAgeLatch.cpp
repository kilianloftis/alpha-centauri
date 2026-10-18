#include "game/population/GoldenAgeLatch.h"

namespace ac
{

GoldenAgeLatch::GoldenAgeLatch(Signal<>& rWillGoldenAge,
                                         Signal<>& rGoldenAgeStarted,
                                         Signal<>& rGoldenAgeEnded)
    : MoodLatch(rWillGoldenAge, rGoldenAgeStarted, rGoldenAgeEnded)
{
}

void GoldenAgeLatch::Commit(bool bGoldenAgeCondition)
{
    Commit_(bGoldenAgeCondition);
}

void GoldenAgeLatch::RestoreState(bool bActive, bool bPending)
{
    Restore_(bActive, bPending);
}

} // namespace ac

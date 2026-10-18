#include "game/population/GoldenAgeLatch.h"

namespace ac
{

GoldenAgeLatch::GoldenAgeLatch(Signal<>& rWillGoldenAge,
                                         Signal<>& rGoldenAgeStarted,
                                         Signal<>& rGoldenAgeEnded)
    : MoodLatch(rWillGoldenAge, rGoldenAgeStarted, rGoldenAgeEnded)
{
}

} // namespace ac

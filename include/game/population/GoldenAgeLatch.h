#pragma once

#include "game/population/MoodLatch.h"

namespace ac
{

// Tracks golden age state for a base population. The condition comes from GoldenAgeConditionMet.
// See MoodLatch for the Forecast / Commit lifecycle this shares with RiotLatch.
class GoldenAgeLatch : public MoodLatch
{
public:
    GoldenAgeLatch(Signal<>& rWillGoldenAge, Signal<>& rGoldenAgeStarted,
                        Signal<>& rGoldenAgeEnded);
    ~GoldenAgeLatch() = default;

    void Commit(bool bGoldenAgeCondition);

    // Save/load without emitting signals.
    void RestoreState(bool bActive, bool bPending);
};

} // namespace ac

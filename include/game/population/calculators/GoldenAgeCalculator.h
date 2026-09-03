#pragma once

#include "game/population/calculators/MoodLatch.h"

namespace ac
{

// Tracks golden age state for a base population.
//
// A golden age needs no drone-class pops at all, and the seated pops' golden age weights to
// reach the base's threshold. Talents weigh +1, plain workers and drones −1, and specialists
// declare no weight — so the shipping rule reads "talents >= workers + drones", with
// specialists neither helping nor hindering. Neither this nor the riot condition is a function
// of base size; both range over the composition pool only.
//
// See MoodLatch for the Forecast / Commit lifecycle this shares with RiotCalculator. Nothing
// forces a golden age, so it adds no bookkeeping of its own.
class GoldenAgeCalculator : public MoodLatch
{
public:
    GoldenAgeCalculator(Signal<>& rWillGoldenAge, Signal<>& rGoldenAgeStarted,
                        Signal<>& rGoldenAgeEnded);
    ~GoldenAgeCalculator() = default;

    struct Inputs_t
    {
        int droneCount    = 0;
        int goldenAgeSum  = 0;
        int threshold     = 0;
    };

    void Forecast(const Inputs_t& rInputs);
    void Commit(const Inputs_t& rInputs);

    // Save/load without emitting signals.
    void RestoreState(bool bActive, bool bPending);

private:
    static bool EvaluateCondition_(const Inputs_t& rInputs);
};

} // namespace ac

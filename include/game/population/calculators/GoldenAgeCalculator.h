#pragma once

#include "lib/Signal.h"

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
// Call Update(inputs) at end of turn to drive golden_age_started / golden_age_ended.
class GoldenAgeCalculator
{
public:
    GoldenAgeCalculator(Signal<>& rGoldenAgeStarted, Signal<>& rGoldenAgeEnded);
    ~GoldenAgeCalculator() = default;

    struct Inputs_t
    {
        int droneCount    = 0;
        int goldenAgeSum  = 0;
        int threshold     = 0;
    };

    // Call at end of turn. Emits golden_age_started when conditions become met,
    // or golden_age_ended when they are no longer met.
    void Update(const Inputs_t& inputs);

    // Returns true if the base is currently in a golden age.
    bool IsInGoldenAge() const;

private:
    Signal<>& m_rGoldenAgeStarted;
    Signal<>& m_rGoldenAgeEnded;
    bool m_bInGoldenAge = false;

    static bool EvaluateCondition_(const Inputs_t& inputs);
};

} // namespace ac

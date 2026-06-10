#pragma once

#include "lib/Signal.h"

namespace ac
{

// Tracks golden age state for a base population.
// A golden age occurs when the base has no Drones and the Talent count
// is at least equal to the combined count of Workers and Specialists.
// Call Update(inputs) at end of turn to drive golden_age_started / golden_age_ended.
class GoldenAgeCalculator
{
public:
    GoldenAgeCalculator(Signal<>& rGoldenAgeStarted, Signal<>& rGoldenAgeEnded);
    ~GoldenAgeCalculator() = default;

    struct Inputs_t
    {
        int droneCount     = 0;
        int talentCount    = 0;
        int workerCount    = 0;
        int specialistCount = 0;
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

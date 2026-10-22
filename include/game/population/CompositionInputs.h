#pragma once

namespace ac
{

class BaseManager;
class DroneCalculator;

// What composition needs resolved against the base's effect list, which only BaseManager can
// build.
struct CompositionEffectInputs_t
{
    // Finalize(StatId_t::Drones): drone *pressure*, not a headcount. Seeded with the three
    // term calculators plus police, with facility / SE Adds and the clamps on top.
    int dronePressure = 0;
    int resolvedTalents = 0;
    // Psych from the current layout, not the stockpile, so mid-turn worker changes count.
    int psychAvailable = 0;
};

// Phase-1 inputs that determine how pops should be seated. Compared on each
// EnsureCompositionCurrent call so composition stays aligned with live base state.
struct CompositionInputKey_t
{
    int dronePressure = 0;
    int resolvedTalents = 0;
    int psychAvailable = 0;
    int poolSize = 0;

    bool operator==(const CompositionInputKey_t&) const = default;
};

// Same shape as ComputeAwayFromHomeDrones / ComputeGarrisonPoliceSuppression, which this
// calls: a base-scoped rule that needs no state of its own.
CompositionEffectInputs_t BuildCompositionInputs(const BaseManager& rBase,
                                                 const DroneCalculator& rDrones);

CompositionInputKey_t ReadCompositionInputKey(const BaseManager& rBase,
                                              const DroneCalculator& rDrones);

} // namespace ac

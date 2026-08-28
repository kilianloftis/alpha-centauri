#pragma once

namespace ac
{

class BaseManager;

// Everything pop composition needs that has to be resolved against a base's effect list.
// Assembled outside PopulationManager because only BaseManager can build that list, and
// outside BaseManager because none of it touches BaseManager's own state.
struct CompositionEffectInputs_t
{
    // Finalize(StatId_t::Drones): drone *pressure*, not a headcount. Seeded with the three
    // term calculators plus police, with facility / SE Adds and the clamps on top.
    int dronePressure = 0;
    int resolvedTalents = 0;
    // Psych available for the ladder this turn, from the current worked layout and pops
    // (BaseManager::GetPsychProduction). Not the psych stockpile — that is fixed after
    // ResourceCollection and would miss mid-turn worker or specialist changes.
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
CompositionEffectInputs_t BuildCompositionInputs(const BaseManager& rBase);

CompositionInputKey_t ReadCompositionInputKey(const BaseManager& rBase);

} // namespace ac

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
    // This turn's psych. Read, never consumed - see ResourceManager::GetPsych.
    int psychAvailable = 0;
};

// Same shape as ComputeAwayFromHomeDrones / ComputeGarrisonPoliceSuppression, which this
// calls: a base-scoped rule that needs no state of its own.
CompositionEffectInputs_t BuildCompositionInputs(const BaseManager& rBase);

} // namespace ac

#pragma once

#include "lib/Signal.h"

namespace ac
{

// Inputs for calculating drone riot condition.
// If targetTalents >= 0, condition is: droneCount > targetTalents
// Otherwise, condition is: droneCount > talentCount (fallback)
struct RiotConditionInputs
{
    int droneCount = 0;
    int talentCount = 0;
    int targetTalents = -1;  // -1 means use talentCount as fallback
};

// Tracks drone riot state for a base population.
// Call NotifyPopGrown(inputs) when the population grows to emit will_riot if conditions are newly met.
// Call Update(inputs) at end of turn to drive is_rioting / riot_ended.
class RiotCalculator
{
public:
    RiotCalculator(Signal<>& rWillRiot, Signal<>& rIsRioting, Signal<>& rRiotEnded);
    ~RiotCalculator() = default;

    // Call after a pop is added. Emits will_riot if conditions are met but riot is not yet active.
    void NotifyPopGrown(const RiotConditionInputs& inputs);

    // Call at end of turn. Emits is_rioting if conditions are met (activates riot),
    // or riot_ended if conditions are no longer met and base was rioting.
    void Update(const RiotConditionInputs& inputs);

    // Force an active drone riot (probe Incite Drone Riots). Emits OnIsRioting if newly active.
    void ForceRiot();

    // True if base is currently in an active drone riot.
    bool IsRioting() const;

private:
    Signal<>& m_rWillRiot;
    Signal<>& m_rIsRioting;
    Signal<>& m_rRiotEnded;
    bool m_bRioting = false;

    static bool ComputeCondition_(const RiotConditionInputs& inputs);
};

} // namespace ac

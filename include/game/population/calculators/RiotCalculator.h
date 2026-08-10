#pragma once

#include "lib/Signal.h"

#include <optional>

namespace ac
{

// Inputs for the natural riot condition: weighted drones outnumber the talent target.
// droneCount is the sum of pop riot_contribution (Super Drone = 2), not head-count.
struct RiotConditionInputs_t
{
    int droneCount = 0;
    int talentCount = 0;
    // The composition's talent target when there is one; otherwise the actual talent count.
    std::optional<int> targetTalents;
};

// Tracks drone riot state for a base population.
//
// Two independent sources keep a riot alive, because they expire differently:
//   - the natural condition (drones > talent target), recomputed on every Update
//   - a forced riot (probe Incite Drone Riots), which lasts a fixed number of turns and which
//     the natural condition cannot sustain, since the action does not change composition
// A base is rioting while either holds.
class RiotCalculator
{
public:
    RiotCalculator(Signal<>& rWillRiot, Signal<>& rIsRioting, Signal<>& rRiotEnded);
    ~RiotCalculator() = default;

    // Call after a pop is added. Emits will_riot if the natural condition is newly met.
    void NotifyPopGrown(const RiotConditionInputs_t& rInputs);

    // Call at end of turn. Ages any forced riot by one turn, then emits is_rioting while the
    // base is rioting, or riot_ended on the turn it stops.
    void Update(const RiotConditionInputs_t& rInputs);

    // Force a riot for the next `turns` end-of-turn passes (probe Incite Drone Riots).
    // Extends but never shortens an existing forced riot. Emits OnIsRioting if newly active.
    void ForceRiot(int turns);

    // True if the base is currently in an active drone riot, from either source.
    bool IsRioting() const;

private:
    static bool NaturalCondition_(const RiotConditionInputs_t& rInputs);

    Signal<>& m_rWillRiot;
    Signal<>& m_rIsRioting;
    Signal<>& m_rRiotEnded;
    bool m_bRioting = false;
    int m_forcedTurnsRemaining = 0;
};

} // namespace ac

#pragma once

#include "lib/Signal.h"

namespace ac
{

// Inputs for the natural riot condition: the seated pops' riot weights against the base's
// threshold. Drones weigh +1 (Super Drones too — a super absorbs two drones of *pressure*, but
// riots like any single citizen), talents −1, and everything else 0.
struct RiotConditionInputs_t
{
    int riotSum = 0;
    int threshold = 0;
};

// Tracks drone riot state for a base population.
//
// Two independent sources keep a riot alive, because they expire differently:
//   - the natural condition (riotSum >= riot_threshold from pop_composition.json), recomputed
//     on every Update
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

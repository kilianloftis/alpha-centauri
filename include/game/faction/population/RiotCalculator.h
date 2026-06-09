#pragma once

#include "lib/Signal.h"

namespace ac
{

// Tracks drone riot state for a base population.
// Call NotifyPopGrown(bCondition) when the population grows to emit will_riot if conditions are newly met.
// Call Update(bCondition) at end of turn to drive is_rioting / riot_ended.
class RiotCalculator
{
public:
    RiotCalculator() = default;
    ~RiotCalculator() = default;

    // Call after a pop is added. Emits will_riot if conditions are met but riot is not yet active.
    void NotifyPopGrown(bool bDroneRiotCondition);

    // Call at end of turn. Emits is_rioting if conditions are met (activates riot),
    // or riot_ended if conditions are no longer met and base was rioting.
    void Update(bool bDroneRiotCondition);

    // True if base is currently in an active drone riot.
    bool IsRioting() const;

    // Signals
    Signal<> will_riot;   // conditions met after growth, riot not yet active
    Signal<> is_rioting;  // end-of-turn: conditions still met, riot now active
    Signal<> riot_ended;  // end-of-turn: conditions no longer met, riot was active

private:
    bool m_bRioting = false;
};

} // namespace ac

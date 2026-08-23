#pragma once

#include "game/faction/base/BaseTypes.h"

#include <map>

namespace ac
{

// One recently-conquered drone window: extra drones decay from peakDrones over durationTurns.
struct AssimilationState
{
    FactionId_t formerOwner = -1;
    int turnsElapsed = 0;
    int durationTurns = 0;
    int peakDrones = 0;

    bool IsActive() const { return durationTurns > 0 && turnsElapsed < durationTurns; }
};

// Occupier penalty plus reclaim claims. Each faction that loses the base while a window is
// still running keeps a claim keyed by their id, so a later recapture (including after a
// third party took the base) inverts that faction's own elapsed time instead of starting a
// fresh 5-drone / 50-turn penalty. Claims expire independently of the current occupier window.
class AssimilationTracker
{
public:
    // peakDrones and decayTurns come from pop_composition.json (shipping 5 and 10).
    // Fresh occupier duration is peakDrones * decayTurns.
    void NotifyCaptured(FactionId_t previousOwner, FactionId_t newOwner, int peakDrones,
                        int decayTurns);
    void Advance();

    const AssimilationState& OccupierWindow() const { return m_occupier; }
    bool IsAssimilating() const { return m_occupier.IsActive(); }

    // Test/query: the claim that would reverse if this faction recaptured now. Inactive if none.
    AssimilationState ClaimFor(FactionId_t factionId) const;

private:
    static AssimilationState MakeFresh_(FactionId_t formerOwner, int peakDrones, int decayTurns);
    static AssimilationState MakeReversed_(FactionId_t formerOwner, int elapsed, int decayTurns);
    static int DecayTurnsOf_(const AssimilationState& rWindow, int fallbackDecay);
    static void Tick_(AssimilationState& rWindow);

    AssimilationState m_occupier;
    std::map<FactionId_t, AssimilationState> m_claims;
};

} // namespace ac

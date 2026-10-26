#pragma once

#include "game/base/BaseTypes.h"

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
    // SMAC's faction_id_former != faction_id: the counter is a conquest still being absorbed.
    bool IsAssimilatingConquest(FactionId_t currentOwner) const
    {
        return formerOwner >= 0 && formerOwner != currentOwner;
    }
};

// The occupier's penalty plus each previous owner's claim, so a recapture inverts that
// faction's elapsed time instead of starting a new penalty.
class AssimilationTracker
{
public:
    // Stores AssimilationOnCapture's answer. peakDrones and decayTurns come from
    // pop_composition.json (shipping 5 and 10).
    void NotifyCaptured(FactionId_t previousOwner, FactionId_t newOwner, int peakDrones,
                        int decayTurns);
    // Stores AssimilationOnIncite's answer. Claims stay put: incite is not an ownership change.
    void NotifyIncited(int baseSize, int decayTurns, int turnsAdded, double capFactor);
    void Advance(FactionId_t currentOwner);

    const AssimilationState& OccupierWindow() const { return m_occupier; }

    // The claim that would reverse if this faction recaptured now. Inactive if none.
    AssimilationState ClaimFor(FactionId_t factionId) const;
    bool HasClaim(FactionId_t factionId) const;

private:
    static void Tick_(AssimilationState& rWindow);

    AssimilationState m_occupier;
    std::map<FactionId_t, AssimilationState> m_claims;
};

} // namespace ac

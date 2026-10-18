#pragma once

#include "game/base/BaseTypes.h"
#include "game/population/AssimilationTracker.h"

#include <optional>

namespace ac
{

class BaseManager;
class Pop;
struct PopCompositionConfig_t;

// Drones weigh +1 to riotSum (Super Drones too: a super absorbs two drones of pressure but
// riots like one citizen), talents -1, everything else 0.
struct RiotConditionInputs_t
{
    int riotSum = 0;
    int forcedTurnsRemaining = 0;
};

struct GoldenAgeConditionInputs_t
{
    int droneCount = 0;
    int goldenAgeSum = 0;
};

// The pop a shrinking base loses: player-chosen types go last, then the one producing least.
// Ties keep the earliest, so the choice does not depend on allocation order.
const Pop& SelectPopToRemove(const BaseManager& rBase);

// A forced riot (probe Incite Drone Riots) holds even when the composition is calm, since the
// action does not change composition.
bool RiotConditionMet(const RiotConditionInputs_t& rInputs, const PopCompositionConfig_t& rConfig);

// Talents weigh +1, plain workers and drones -1, specialists nothing, and any drone rules it out.
bool GoldenAgeConditionMet(const GoldenAgeConditionInputs_t& rInputs,
                           const PopCompositionConfig_t& rConfig);

// Forcing a riot extends one already running but never shortens it.
int ForcedRiotTurns(int remaining, int added);

struct AssimilationOnCapture_t
{
    AssimilationState occupier;
    // The new owner's own claim became the occupier window.
    bool bReclaims = false;
    // Set only when the previous owner holds no claim yet.
    std::optional<AssimilationState> previousOwnerClaim;
};

// A recapture inverts the new owner's own elapsed time instead of starting a fresh window, and
// an earlier owner's claim survives chained captures.
AssimilationOnCapture_t AssimilationOnCapture(const AssimilationTracker& rTracker,
                                              FactionId_t previousOwner, FactionId_t newOwner,
                                              int peakDrones, int decayTurns);

} // namespace ac

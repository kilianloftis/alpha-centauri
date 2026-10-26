#include "game/population/PopulationRules.h"

#include "game/base/BaseManager.h"
#include "game/base/BaseTypes.h"
#include "game/map/Tile.h"
#include "game/population/Pop.h"
#include "game/population/PopCompositionConfigParser.h"
#include "game/population/PopulationManager.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ac
{

namespace
{

int PopValue_(const BaseManager& rBase, const Pop& rPop)
{
    if (!rPop.IsWorker())
    {
        const SpecialistOutput_t output = rPop.GetSpecialistOutput();
        return output.econ + output.labs + output.psych;
    }
    const Tile* pTile = rPop.GetTile();
    if (!pTile)
    {
        // An unassigned worker produces nothing, so it is the first to go.
        return 0;
    }
    const TileResources_t yield = rBase.GetWorkedTileYield(*pTile).effective;
    return yield.nutrients + yield.minerals + yield.energy;
}

AssimilationState FreshWindow_(FactionId_t formerOwner, int peakDrones, int decayTurns)
{
    AssimilationState state;
    if (peakDrones <= 0 || decayTurns <= 0)
    {
        return state;
    }
    state.formerOwner = formerOwner;
    state.peakDrones = peakDrones;
    state.durationTurns = peakDrones * decayTurns;
    return state;
}

AssimilationState ReversedWindow_(FactionId_t formerOwner, int elapsed, int decayTurns)
{
    AssimilationState state;
    if (elapsed <= 0 || decayTurns <= 0)
    {
        return state;
    }
    const int reversedPeak = elapsed / decayTurns;
    if (reversedPeak <= 0)
    {
        return state;
    }
    state.formerOwner = formerOwner;
    state.peakDrones = reversedPeak;
    state.durationTurns = elapsed;
    return state;
}

int DecayTurnsOf_(const AssimilationState& rWindow, int fallbackDecay)
{
    if (rWindow.peakDrones <= 0)
    {
        return std::max(1, fallbackDecay);
    }
    return std::max(1, rWindow.durationTurns / rWindow.peakDrones);
}

int RemainingTurns_(const AssimilationState& rOccupier)
{
    if (!rOccupier.IsActive())
    {
        return 0;
    }
    return rOccupier.durationTurns - rOccupier.turnsElapsed;
}

int SizeCapTurns_(int baseSize, double capFactor)
{
    const double raw = capFactor * static_cast<double>(baseSize);
    return static_cast<int>(std::floor(raw));
}

// The occupation formula then yields ceil(turnsLeft / decay) drones, SMAC's (assim + 9) / 10.
AssimilationState WindowWithTurnsLeft_(FactionId_t formerOwner, int turnsLeft, int decayTurns)
{
    AssimilationState state;
    if (turnsLeft <= 0)
    {
        return state;
    }
    state.formerOwner = formerOwner;
    state.peakDrones = (turnsLeft + decayTurns - 1) / decayTurns;
    state.durationTurns = state.peakDrones * decayTurns;
    state.turnsElapsed = state.durationTurns - turnsLeft;
    return state;
}

} // namespace

const Pop& SelectPopToRemove(const BaseManager& rBase)
{
    const Pop* pDoomed = nullptr;
    std::pair<bool, int> worst;
    for (const Pop& rPop : rBase.GetPopulation().Pops())
    {
        const std::pair<bool, int> key{rPop.IsPlayerChoiceType(), PopValue_(rBase, rPop)};
        if (!pDoomed || key < worst)
        {
            pDoomed = &rPop;
            worst = key;
        }
    }
    if (!pDoomed)
    {
        throw std::runtime_error("SelectPopToRemove: base has no population");
    }
    return *pDoomed;
}

bool RiotConditionMet(const RiotConditionInputs_t& rInputs, const PopCompositionConfig_t& rConfig)
{
    return rInputs.riotSum >= rConfig.riotThreshold;
}

bool GoldenAgeConditionMet(const GoldenAgeConditionInputs_t& rInputs,
                           const PopCompositionConfig_t& rConfig)
{
    return rInputs.droneCount == 0 && rInputs.goldenAgeSum >= rConfig.goldenAgeThreshold;
}

AssimilationOnCapture_t AssimilationOnCapture(const AssimilationTracker& rTracker,
                                              FactionId_t previousOwner, FactionId_t newOwner,
                                              int peakDrones, int decayTurns)
{
    AssimilationOnCapture_t outcome;
    const AssimilationState claim = rTracker.ClaimFor(newOwner);
    if (claim.IsActive())
    {
        const int decay = DecayTurnsOf_(claim, decayTurns);
        outcome.bReclaims = true;
        outcome.occupier = ReversedWindow_(previousOwner, claim.turnsElapsed, decay);
    }
    else
    {
        outcome.occupier = FreshWindow_(previousOwner, peakDrones, decayTurns);
    }

    // A's elapsed time while B and then C occupy must not reset at C's capture.
    if (!rTracker.HasClaim(previousOwner))
    {
        outcome.previousOwnerClaim = FreshWindow_(previousOwner, peakDrones, decayTurns);
    }
    return outcome;
}

AssimilationState AssimilationOnIncite(const AssimilationState& rOccupier, int baseSize,
                                       int decayTurns, int turnsAdded, double capFactor)
{
    const int turnsLeft = RemainingTurns_(rOccupier);
    const int cap = std::max(turnsLeft, SizeCapTurns_(baseSize, capFactor));
    const int bumped = std::clamp(turnsLeft + turnsAdded, 0, cap);
    if (bumped == turnsLeft)
    {
        return rOccupier;
    }
    const int decay = DecayTurnsOf_(rOccupier, decayTurns);
    return WindowWithTurnsLeft_(rOccupier.formerOwner, bumped, decay);
}

int OccupierDecayTicks(const AssimilationState& rOccupier, FactionId_t currentOwner)
{
    if (rOccupier.IsAssimilatingConquest(currentOwner))
    {
        return 1;
    }
    return 2;
}

} // namespace ac

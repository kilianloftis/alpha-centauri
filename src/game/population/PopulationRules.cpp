#include "game/population/PopulationRules.h"

#include "game/base/BaseManager.h"
#include "game/base/BaseTypes.h"
#include "game/map/Tile.h"
#include "game/population/Pop.h"
#include "game/population/PopCompositionConfigParser.h"
#include "game/population/PopulationManager.h"

#include <algorithm>
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
    return rInputs.riotSum >= rConfig.riotThreshold || rInputs.forcedTurnsRemaining > 0;
}

bool GoldenAgeConditionMet(const GoldenAgeConditionInputs_t& rInputs,
                           const PopCompositionConfig_t& rConfig)
{
    return rInputs.droneCount == 0 && rInputs.goldenAgeSum >= rConfig.goldenAgeThreshold;
}

int ForcedRiotTurns(int remaining, int added)
{
    return std::max(remaining, std::max(0, added));
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

} // namespace ac

#include "game/faction/base/population/AssimilationTracker.h"

#include <algorithm>

namespace ac
{

AssimilationState AssimilationTracker::MakeFresh_(FactionId_t formerOwner, int peakDrones,
                                                 int decayTurns)
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

AssimilationState AssimilationTracker::MakeReversed_(FactionId_t formerOwner, int elapsed,
                                                    int decayTurns)
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

int AssimilationTracker::DecayTurnsOf_(const AssimilationState& rWindow, int fallbackDecay)
{
    if (rWindow.peakDrones <= 0)
    {
        return std::max(1, fallbackDecay);
    }
    return std::max(1, rWindow.durationTurns / rWindow.peakDrones);
}

void AssimilationTracker::Tick_(AssimilationState& rWindow)
{
    if (!rWindow.IsActive())
    {
        rWindow = AssimilationState{};
        return;
    }
    ++rWindow.turnsElapsed;
    if (!rWindow.IsActive())
    {
        rWindow = AssimilationState{};
    }
}

void AssimilationTracker::NotifyCaptured(FactionId_t previousOwner, FactionId_t newOwner,
                                        int peakDrones, int decayTurns)
{
    const auto it = m_claims.find(newOwner);
    if (it != m_claims.end() && it->second.IsActive())
    {
        const int elapsed = it->second.turnsElapsed;
        const int decay = DecayTurnsOf_(it->second, decayTurns);
        m_claims.erase(it);
        m_occupier = MakeReversed_(previousOwner, elapsed, decay);
    }
    else
    {
        m_occupier = MakeFresh_(previousOwner, peakDrones, decayTurns);
    }

    // Preserve an existing claim: A's elapsed while B then C occupy must not reset at C's capture.
    if (m_claims.find(previousOwner) == m_claims.end())
    {
        m_claims[previousOwner] = MakeFresh_(previousOwner, peakDrones, decayTurns);
    }
}

void AssimilationTracker::Advance()
{
    Tick_(m_occupier);
    for (auto it = m_claims.begin(); it != m_claims.end();)
    {
        Tick_(it->second);
        if (!it->second.IsActive())
        {
            it = m_claims.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

AssimilationState AssimilationTracker::ClaimFor(FactionId_t factionId) const
{
    const auto it = m_claims.find(factionId);
    if (it == m_claims.end())
    {
        return {};
    }
    return it->second;
}

} // namespace ac

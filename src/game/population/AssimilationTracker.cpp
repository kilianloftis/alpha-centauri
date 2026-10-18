#include "game/population/AssimilationTracker.h"

#include "game/population/PopulationRules.h"

#include <algorithm>

namespace ac
{

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
    const AssimilationOnCapture_t outcome =
        AssimilationOnCapture(*this, previousOwner, newOwner, peakDrones, decayTurns);
    if (outcome.bReclaims)
    {
        m_claims.erase(newOwner);
    }
    m_occupier = outcome.occupier;
    if (outcome.previousOwnerClaim)
    {
        m_claims[previousOwner] = *outcome.previousOwnerClaim;
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

bool AssimilationTracker::HasClaim(FactionId_t factionId) const
{
    return m_claims.contains(factionId);
}

} // namespace ac

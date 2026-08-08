#include "ui/council/CouncilVoteWeightCache.h"

#include "game/Faction.h"
#include "game/council/PlanetaryCouncil.h"

namespace ac
{

int CouncilVoteWeightCache::Get(const PlanetaryCouncil& rCouncil, const Faction& rFaction,
                                CouncilVoteWeight_t mode)
{
    Entry_t candidate;
    candidate.bValid = true;
    candidate.councilRevision = rCouncil.GetRevision().Get();
    candidate.localEffectsVersion = rFaction.GetLocalEffectsVersion();
    candidate.population = rFaction.TotalPopulation();
    candidate.mode = mode;

    Entry_t& rEntry = m_entries[rFaction.GetFactionId()];
    if (rEntry.bValid
        && rEntry.councilRevision == candidate.councilRevision
        && rEntry.localEffectsVersion == candidate.localEffectsVersion
        && rEntry.population == candidate.population
        && rEntry.mode == candidate.mode)
    {
        return rEntry.weight;
    }

    ++m_computeCount;
    candidate.weight = rCouncil.ComputeVoteWeight(rFaction, mode);
    rEntry = candidate;
    return rEntry.weight;
}

} // namespace ac

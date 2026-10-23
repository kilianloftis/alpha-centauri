#pragma once

#include "game/council/CouncilProposalConfig.h"
#include "game/base/BaseTypes.h"

#include <cstdint>
#include <unordered_map>

namespace ac
{

class Faction;
class PlanetaryCouncil;

// Caches VoteWeight, which resolves the faction's whole effect pool and is asked for once per
// member per paint. Keyed on the council revision, the faction's effects version and population.
class CouncilVoteWeightCache
{
public:
    int Get(const PlanetaryCouncil& rCouncil, const Faction& rFaction, CouncilVoteWeight_t mode);

private:
    struct Entry_t
    {
        // Every counter legitimately starts at zero, so a default-constructed entry would
        // otherwise read as a valid cached weight of 0.
        bool bValid = false;
        uint64_t councilRevision = 0;
        uint64_t localEffectsVersion = 0;
        int population = 0;
        CouncilVoteWeight_t mode = CouncilVoteWeight_t::Representative;
        int weight = 0;
    };

    std::unordered_map<FactionId_t, Entry_t> m_entries;
};

} // namespace ac

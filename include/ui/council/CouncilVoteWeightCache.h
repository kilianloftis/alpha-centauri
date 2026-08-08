#pragma once

#include "game/council/CouncilProposalConfig.h"
#include "game/faction/base/BaseTypes.h"

#include <cstdint>
#include <unordered_map>

namespace ac
{

class Faction;
class PlanetaryCouncil;

// PlanetaryCouncil::ComputeVoteWeight copies the faction's whole effect pool, appends the
// council's and the world's, and resolves CouncilVotes modifiers over it. The panels ask for it
// once per member per paint, so it is cached against the three things that move it: the council
// revision, the faction's own effects version, and its population.
//
// Population is compared directly because no counter tracks it. Every population change also
// moves the effect-pool version in practice, so the field is defensive; it costs one int
// compare and removes the dependency on that coupling.
class CouncilVoteWeightCache
{
public:
    int Get(const PlanetaryCouncil& rCouncil, const Faction& rFaction, CouncilVoteWeight_t mode);

    // How many times the underlying resolve actually ran. The saved work is the entire point
    // of this class, and it is not observable from the returned weights alone.
    uint64_t GetComputeCount() const { return m_computeCount; }

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
    uint64_t m_computeCount = 0;
};

} // namespace ac

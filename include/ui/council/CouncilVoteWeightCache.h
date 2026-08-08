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
// council's and the world's, and resolves CouncilVotes modifiers over it. Both council panels
// called it once per member per paint — and the info panel calls it a second time for the
// tally, so a five-faction council resolved the stat fifteen times per frame for a number that
// only moves when the council rebuilds, a faction's own effects change, or its population does.
//
// Keyed on exactly those three inputs. Population is compared directly rather than through a
// revision because no counter tracks it. In practice every population change also moves the
// faction's effect-pool version (pops contribute effects), so the population field is defensive
// rather than load-bearing — it costs one int compare and removes the need to rely on that.
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

#pragma once

#include "game/base/BaseTypes.h"
#include "lib/Revision.h"

#include <cstdint>
#include <map>

namespace ac
{

// Each faction's ecological history: fungal blooms, clean-mineral grants since the first bloom,
// and virtual minerals. GameState owns one.
class EcologyLedger
{
public:
    EcologyLedger() = default;

    void RecordFungalBloom(FactionId_t faction);
    // amount must be positive.
    void GrantCleanMinerals(FactionId_t faction, int amount);
    // amount must be positive.
    void AddVirtualMinerals(FactionId_t faction, int amount);

    // Each is 0 for a faction with nothing recorded.
    int FungalBlooms(FactionId_t faction) const;
    int CleanMineralGrants(FactionId_t faction) const;
    int VirtualMinerals(FactionId_t faction) const;

    // Moves on every write. The eco-damage memo validates against it: none of these writes
    // touches an effect pool.
    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    struct Tallies_t
    {
        int fungalBlooms = 0;
        int cleanMineralGrants = 0;
        int virtualMinerals = 0;
    };

    Tallies_t Find_(FactionId_t faction) const;

    Revision m_revision;
    std::map<FactionId_t, Tallies_t> m_tallies;
};

} // namespace ac

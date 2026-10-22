#pragma once

#include "game/economy/CommerceRules.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;

// The owner's per-base commerce breakdown, memoized since pairing is planet-wide and the
// callers are hot. Validated element-wise against its revisions.
class CommerceCache
{
public:
    explicit CommerceCache(const Faction& rOwner);
    ~CommerceCache() = default;

    CommerceCache(const CommerceCache&) = delete;
    CommerceCache& operator=(const CommerceCache&) = delete;

    // Partner breakdown for rBase. Empty when the owner has no bound GameState or no
    // Trading pairs that include this base.
    const std::vector<CommercePartnerLine_t>& ComputeForBase(const BaseManager& rBase) const;

    // Commerce energy rBase earns this turn: its lines summed, negatives dropped so a line
    // can never charge a base for a treaty.
    int GetCommerceEnergy(const BaseManager& rBase) const;

private:
    // Everything pairing reads that moves independently: worker placement, improvements,
    // treaties, sanctions and each faction's effects version.
    void CollectRevisions_(std::vector<uint64_t>& rOut) const;
    void Validate_() const;

    const Faction& m_rOwner;

    mutable std::unordered_map<BaseId_t, std::vector<CommercePartnerLine_t>> m_cached;
    // Seeded to a value CollectRevisions_ can never produce, so the first query builds.
    mutable std::vector<uint64_t> m_cachedStamp{UINT64_MAX};
    mutable std::vector<uint64_t> m_scratchRevisions;
};

} // namespace ac

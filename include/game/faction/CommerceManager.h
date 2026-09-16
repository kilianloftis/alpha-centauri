#pragma once

#include "game/faction/CommerceCalculator.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;

// Faction-owned commerce queries. Pairing math stays in CommerceCalculator; this façade is
// injected into each base ResourceManager (like EconomyManager) and used by CommerceDisplay.
//
// It memoizes the owner's whole per-base breakdown, because the underlying pass is
// planet-wide (it prices every base of every treaty partner) while the callers are per-base
// and hot: ResourceManager::AllocatableEnergy_ backs econ/labs/psych, so composition's
// EnsureCompositionCurrent key and every base-screen frame land here. The memo is validated
// the way FactionEffectsPool validates its pool — an element-wise compare of the revisions
// it was built from, never a hash of them.
class CommerceManager
{
public:
    explicit CommerceManager(const Faction& rOwner);
    ~CommerceManager() = default;

    CommerceManager(const CommerceManager&) = delete;
    CommerceManager& operator=(const CommerceManager&) = delete;

    // Partner breakdown for rBase. Empty when the owner has no bound GameState or no
    // Friendship/Pact pairs that include this base.
    const std::vector<CommercePartnerLine_t>& ComputeForBase(const BaseManager& rBase) const;

    // Commerce energy rBase earns this turn: its lines summed, negatives dropped so a line
    // can never charge a base for a treaty.
    int GetCommerceEnergy(const BaseManager& rBase) const;

private:
    // Everything the pairing pass reads that can move without the others moving: worker
    // placement and tile improvements change base energy without touching any effect pool,
    // treaties gate the pairs, and each faction's effects version already folds in its base
    // list, buildings, pops, research and social engineering.
    void CollectRevisions_(std::vector<uint64_t>& rOut) const;
    void Validate_() const;

    const Faction& m_rOwner;
    CommerceCalculator m_calculator;

    mutable std::unordered_map<BaseId_t, std::vector<CommercePartnerLine_t>> m_cached;
    // Seeded to a value CollectRevisions_ can never produce, so the first query builds.
    mutable std::vector<uint64_t> m_cachedStamp{UINT64_MAX};
    mutable std::vector<uint64_t> m_scratchRevisions;
};

} // namespace ac

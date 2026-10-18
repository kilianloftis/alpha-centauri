#include "game/economy/CommerceCache.h"

#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/CurrentGameState.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/base/BaseManager.h"
#include "game/map/WorldMap.h"

namespace ac
{
namespace
{

const std::vector<CommercePartnerLine_t>& NoLines_()
{
    static const std::vector<CommercePartnerLine_t> k_empty;
    return k_empty;
}

} // namespace

CommerceCache::CommerceCache(const Faction& rOwner)
    : m_rOwner(rOwner)
{}

void CommerceCache::CollectRevisions_(std::vector<uint64_t>& rOut) const
{
    rOut.clear();

    const GameState* pState = FindCurrentGameState();
    if (pState == nullptr)
    {
        // No game: no commerce, and nothing to key on. Distinct from the seeded stamp, so the
        // first query still builds (into an empty map) and settles.
        return;
    }

    // A different current game state must not hit this cache.
    rOut.push_back(reinterpret_cast<uintptr_t>(pState));
    const WorldMap& rMap = pState->GetWorldMap();
    rOut.push_back(rMap.GetWorkedTiles().GetRevision());
    rOut.push_back(rMap.GetAppearanceRevision());
    rOut.push_back(pState->GetDiplomacyLedger().GetRevision());
    // Sanctions zero a pair without touching any treaty or effect pool, so the memo has to key
    // on them too — otherwise an atrocity keeps paying commerce for the rest of the turn.
    rOut.push_back(pState->GetAtrocityLedger().GetRevision());
    // Every faction, not just the owner: a partner's bases are priced into our pairs.
    for (const Faction& rFaction : pState->Factions())
    {
        rOut.push_back(rFaction.GetEffectsVersion());
    }
}

void CommerceCache::Validate_() const
{
    CollectRevisions_(m_scratchRevisions);
    if (m_scratchRevisions == m_cachedStamp)
    {
        return;
    }

    if (FindCurrentGameState() == nullptr)
    {
        m_cached.clear();
    }
    else
    {
        m_cached = ComputeCommerceLines(m_rOwner);
    }
    m_cachedStamp = m_scratchRevisions;
}

const std::vector<CommercePartnerLine_t>& CommerceCache::ComputeForBase(
    const BaseManager& rBase) const
{
    Validate_();
    const auto it = m_cached.find(rBase.GetBaseId());
    if (it == m_cached.end())
    {
        return NoLines_();
    }
    return it->second;
}

int CommerceCache::GetCommerceEnergy(const BaseManager& rBase) const
{
    int total = 0;
    for (const CommercePartnerLine_t& rLine : ComputeForBase(rBase))
    {
        if (rLine.ourEnergy > 0)
        {
            total += rLine.ourEnergy;
        }
    }
    return total;
}

} // namespace ac

#include "game/faction/CommerceManager.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/base/BaseManager.h"
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

CommerceManager::CommerceManager(const Faction& rOwner)
    : m_rOwner(rOwner)
{}

void CommerceManager::CollectRevisions_(std::vector<uint64_t>& rOut) const
{
    rOut.clear();

    const GameState* pState = m_rOwner.GetGameState();
    if (pState == nullptr)
    {
        // Unbound faction: no commerce, and nothing to key on. Distinct from the seeded
        // stamp, so the first query still builds (into an empty map) and settles.
        return;
    }

    const WorldMap& rMap = pState->GetWorldMap();
    rOut.push_back(rMap.GetWorkedTiles().GetRevision());
    rOut.push_back(rMap.GetAppearanceRevision());
    rOut.push_back(pState->GetDiplomacyLedger().GetRevision());
    // Every faction, not just the owner: a partner's bases are priced into our pairs.
    for (const Faction& rFaction : pState->Factions())
    {
        rOut.push_back(rFaction.GetEffectsVersion());
    }
}

void CommerceManager::Validate_() const
{
    CollectRevisions_(m_scratchRevisions);
    if (m_scratchRevisions == m_cachedStamp)
    {
        return;
    }

    const GameState* pState = m_rOwner.GetGameState();
    if (pState == nullptr)
    {
        m_cached.clear();
    }
    else
    {
        m_cached = m_calculator.ComputeAllLines(m_rOwner, *pState);
    }
    m_cachedStamp = m_scratchRevisions;
}

const std::vector<CommercePartnerLine_t>& CommerceManager::ComputeForBase(
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

int CommerceManager::GetCommerceEnergy(const BaseManager& rBase) const
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

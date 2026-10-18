#include "game/ecology/BaseEcology.h"

#include "game/ecology/EcologyRules.h"

#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/CurrentGameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/research/ResearchManager.h"
#include "game/base/BaseManager.h"
#include "game/population/PopulationManager.h"
#include "game/economy/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <cstdint>
#include <vector>

namespace ac
{

BaseEcology::BaseEcology(const BaseManager& rBase, const EcoDamageCalculator& rCalculator)
    : m_rBase(rBase)
    , m_rCalculator(rCalculator)
{
}

int BaseEcology::GetDamage() const
{
    const GameState& rGameState = CurrentGameState();
    CollectRevisions_(rGameState, m_scratchRevisions);
    if (m_scratchRevisions != m_cachedStamp)
    {
        const EcoDamageInputs_t inputs = EcoDamageInputs(m_rBase);
        m_cachedDamage = m_rCalculator.Calculate(inputs);
        m_cachedStamp = m_scratchRevisions;
    }
    return m_cachedDamage;
}

void BaseEcology::CollectRevisions_(const GameState& rGameState, std::vector<uint64_t>& rOut) const
{
    const Faction& rFaction = m_rBase.GetFaction();
    const WorldMap& rMap = m_rBase.GetTileEffects().GetWorldMap();
    rOut.clear();
    // A different current game state must not hit this cache.
    rOut.push_back(reinterpret_cast<uintptr_t>(&rGameState));
    rOut.push_back(rFaction.GetFactionId());
    rOut.push_back(rFaction.GetEffectsVersion());
    rOut.push_back(rFaction.GetResearch().GetRevision());
    rOut.push_back(m_rBase.GetPopulation().GetRevision());
    rOut.push_back(m_rBase.GetPopulation().GetMoodRevision());
    rOut.push_back(rMap.GetWorkedTiles().GetRevision());
    rOut.push_back(rMap.GetAppearanceRevision());
    rOut.push_back(rGameState.GetAtrocityLedger().GetRevision());
    rOut.push_back(rGameState.GetEcologyLedger().GetRevision());
}

} // namespace ac

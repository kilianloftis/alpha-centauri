#include "game/stages/YieldingPerFactionTurnStage.h"

#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/PlayerInteractionQueue.h"

#include <utility>

namespace ac
{

void YieldingPerFactionTurnStage::EnsureActiveFaction_(const Faction& rFaction)
{
    const FactionId_t factionId = rFaction.GetFactionId();
    if (m_activeFactionId.has_value() && *m_activeFactionId != factionId)
    {
        // Prior pass abandoned (e.g. resume faction eliminated while yielded).
        ResetPassState_();
    }
    m_activeFactionId = factionId;
}

void YieldingPerFactionTurnStage::ResetPassState_()
{
    m_activeFactionId.reset();
    OnResetPassState_();
}

bool YieldingPerFactionTurnStage::PlayerHasPending_(const GameState& rGameState)
{
    const FactionId_t playerId = rGameState.GetPlayerFaction().GetFactionId();
    return rGameState.GetPlayerInteractions().HasPendingFor(playerId);
}

} // namespace ac

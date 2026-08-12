#include "game/stages/YieldingPerFactionTurnStage.h"

#include "game/Faction.h"
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
    const Faction* pPlayer = rGameState.GetPlayerFaction();
    if (!pPlayer)
    {
        return false;
    }
    return rGameState.GetPlayerInteractions().HasPendingFor(pPlayer->GetFactionId());
}

void YieldingPerFactionTurnStage::EnqueueForPlayer_(GameState& rGameState,
                                                    PlayerInteraction_t payload)
{
    const Faction* pPlayer = rGameState.GetPlayerFaction();
    if (!pPlayer)
    {
        return;
    }
    rGameState.GetPlayerInteractions().Enqueue(
        QueuedInteraction_t{std::move(payload), pPlayer->GetFactionId()});
}

} // namespace ac

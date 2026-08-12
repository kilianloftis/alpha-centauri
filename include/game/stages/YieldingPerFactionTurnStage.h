#pragma once

#include "game/PlayerInteraction.h"
#include "game/TurnStages.h"
#include "game/faction/base/BaseTypes.h"

#include <optional>

namespace ac
{

class Faction;
class GameState;

// Shared pass-lifecycle for per-faction stages that Yield for the player-interaction queue.
// Does not unify entity iteration (bases vs units stay in subclasses).
class YieldingPerFactionTurnStage : public PerFactionTurnStage
{
public:
    using PerFactionTurnStage::PerFactionTurnStage;

protected:
    void EnsureActiveFaction_(const Faction& rFaction);
    void ResetPassState_();

    // True when any interaction for the player faction is still queued (any stage may have
    // enqueued it). Callers Yield so the queue drains before more state changes.
    static bool PlayerHasPending_(const GameState& rGameState);

    // Queue an interaction for the player faction. No-op when there is no player faction.
    static void EnqueueForPlayer_(GameState& rGameState, PlayerInteraction_t payload);

    // Subclasses clear stage-local resume state here; m_activeFactionId is cleared by ResetPassState_.
    virtual void OnResetPassState_() {}

    std::optional<FactionId_t> m_activeFactionId;
};

} // namespace ac

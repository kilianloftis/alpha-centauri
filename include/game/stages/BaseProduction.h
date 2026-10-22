#pragma once

#include "game/stages/YieldingPerFactionTurnStage.h"
#include "game/PlayerInteraction.h"
#include "game/base/BaseManager.h"
#include "game/production/ProductionApplyResult.h"

#include <optional>
#include <unordered_set>

namespace ac
{

class Faction;

// Per-faction production. Owns only turn-pass resume state (which bases already ticked).
// Default TickBase_ banks leftover minerals (ApplyProduction), then completes if the queue
// is still constructable (TryCompleteReady, new turn). PostActionsProduction overrides
// TickBase_ for completion-only without clearing a same-turn abandon deferral.
// Production rules and the pending confirmation live on BaseManager / ProductionManager;
// queue + presenter own the UI prompt.
class BaseProduction : public YieldingPerFactionTurnStage
{
public:
    explicit BaseProduction(HookContext hookContext);
    ~BaseProduction() override = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
    void OnExitImpl() override;
    void OnResetPassState_() override;

    // Per-base production tick for this pass. Override for completion-only late passes.
    virtual ProductionApplyResult_t TickBase_(BaseManager& rBase);
    // Finish a funded queue that is still constructable, without banking or clearing a
    // same-turn abandon deferral. Used on resume and by PostActionsProduction.
    ProductionApplyResult_t TryCompleteIfConstructable_(BaseManager& rBase);

private:
    // Dropped item when the queue is occupied but no longer constructable. Empty queue is
    // Idle, not pre-empted.
    static std::optional<ProductionPreempt_t> UnconstructableQueue_(const BaseManager& rBase);
    // Continue = this base is done for the pass; Yield = stop and wait on the player.
    StageResult_t ProcessBase_(GameState& rGameState, Faction& rFaction, BaseManager& rBase);
    StageResult_t HandleApplyResult_(GameState& rGameState, Faction& rFaction, BaseManager& rBase,
                                     const ProductionApplyResult_t& rResult);
    StageResult_t ReevaluateProcessedBases_(GameState& rGameState, Faction& rFaction);
    StageResult_t HandlePreempted_(GameState& rGameState, Faction& rFaction, BaseManager& rBase,
                                  const ProductionPreempt_t& rPreempted);
    StageResult_t HandleAwaitingConfirmation_(GameState& rGameState, Faction& rFaction,
                                             BaseManager& rBase);
    StageResult_t HandleProductionCompleted_(GameState& rGameState, Faction& rFaction,
                                            BaseManager& rBase,
                                            const ProductionApplyResult_t& rResult);
    static void LogProductionTick_(const BaseManager& rBase, const ProductionApplyResult_t& rResult);

    std::unordered_set<BaseId_t> m_processedBaseIds;
};

} // namespace ac

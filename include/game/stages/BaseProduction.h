#pragma once

#include "game/stages/YieldingPerFactionTurnStage.h"
#include "game/PlayerInteraction.h"
#include "game/base/BaseManager.h"
#include "game/production/ProductionApplyResult.h"

#include <unordered_set>

namespace ac
{

class Faction;

// Per-faction production: bank leftovers, complete if ready, validate the queue. Keeps only
// which bases this pass has ticked.
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
    // ValidateQueue after a completion attempt; a dropped current item is attached as preempted.
    ProductionApplyResult_t ValidateQueueAfter_(BaseManager& rBase, ProductionApplyResult_t result);

private:
    StageResult_t ProcessBase_(GameState& rGameState, Faction& rFaction, BaseManager& rBase);
    StageResult_t HandleApplyResult_(GameState& rGameState, Faction& rFaction, BaseManager& rBase,
                                     const ProductionApplyResult_t& rResult);
    StageResult_t ReevaluateProcessedBases_(GameState& rGameState, Faction& rFaction);
    StageResult_t HandleAwaitingConfirmation_(GameState& rGameState, Faction& rFaction,
                                             BaseManager& rBase);
    StageResult_t HandlePreempted_(GameState& rGameState, Faction& rFaction, BaseManager& rBase,
                                   const ProductionPreempt_t& rPreempted);
    StageResult_t HandleProductionCompleted_(GameState& rGameState, Faction& rFaction,
                                            BaseManager& rBase,
                                            const ProductionApplyResult_t& rResult);
    static void LogProductionTick_(const BaseManager& rBase, const ProductionApplyResult_t& rResult);

    std::unordered_set<BaseId_t> m_processedBaseIds;
};

} // namespace ac

#pragma once

#include "game/stages/YieldingPerFactionTurnStage.h"
#include "game/PlayerInteraction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionApplyResult.h"

#include <unordered_set>

namespace ac
{

class Faction;

// Per-faction production completion. Owns only turn-pass resume state (which bases already
// ticked). Minerals were already claimed by MineralConversion. Production rules and abandon
// pending live on BaseManager; queue + presenter own the UI prompt.
class BaseProduction : public YieldingPerFactionTurnStage
{
public:
    explicit BaseProduction(HookContext hookContext);
    ~BaseProduction() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
    void OnExitImpl() override;
    void OnResetPassState_() override;

private:
    // Continue = this base is done for the pass; Yield = stop and wait on the player.
    StageResult_t ProcessBase_(GameState& rGameState, Faction& rFaction, BaseManager& rBase);
    StageResult_t HandleApplyResult_(GameState& rGameState, Faction& rFaction, BaseManager& rBase,
                                     const ProductionApplyResult_t& rResult);
    StageResult_t ReevaluateProcessedBases_(GameState& rGameState, Faction& rFaction);
    StageResult_t HandleAbandonConfirm_(GameState& rGameState, Faction& rFaction, BaseManager& rBase);
    StageResult_t HandleProductionCompleted_(GameState& rGameState, Faction& rFaction,
                                            BaseManager& rBase,
                                            const ProductionApplyResult_t& rResult);
    static void LogProductionTick_(const BaseManager& rBase, const ProductionApplyResult_t& rResult);

    std::unordered_set<BaseId_t> m_processedBaseIds;
};

} // namespace ac

#pragma once

#include "game/stages/YieldingPerFactionTurnStage.h"
#include "game/PlayerInteraction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionApplyResult.h"

#include <optional>
#include <unordered_set>

namespace ac
{

class Faction;

// Per-faction mineral banking / completion. Owns only turn-pass resume state (which bases
// already ticked, which base is waiting on a player interaction). Production rules
// and abandon pending live on BaseManager; queue + presenter own the UI prompt.
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
    StageResult_t HandleAbandonConfirm_(GameState& rGameState, Faction& rFaction, BaseManager& rBase);
    StageResult_t HandleProductionCompleted_(GameState& rGameState, Faction& rFaction,
                                            BaseManager& rBase,
                                            const ProductionApplyResult_t& rResult);
    static void LogProductionTick_(const BaseManager& rBase, const ProductionApplyResult_t& rResult);

    std::unordered_set<BaseId_t> m_processedBaseIds;
    std::optional<BaseId_t> m_awaitingBaseId;
};

} // namespace ac

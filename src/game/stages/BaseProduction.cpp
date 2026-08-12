#include "game/stages/BaseProduction.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/IConstructable.h"
#include "game/PlayerInteraction.h"
#include "game/PlayerInteractionQueue.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/map/Tile.h"
#include "game/TurnStageRegistrar.h"

#include <iostream>
#include <utility>

namespace ac
{

namespace { TurnStageRegistrar<BaseProduction> g_registrar("BaseProduction"); }

BaseProduction::BaseProduction(HookContext hookContext)
    : YieldingPerFactionTurnStage(std::move(hookContext))
{
}

void BaseProduction::OnResetPassState_()
{
    m_processedBaseIds.clear();
    m_awaitingBaseId.reset();
}

void BaseProduction::OnExitImpl()
{
    ResetPassState_();
}

StageResult_t BaseProduction::HandleAbandonConfirm_(GameState& rGameState, Faction& rFaction,
                                                   BaseManager& rBase)
{
    if (!rFaction.IsPlayerControlled())
    {
        // TODO: the real rule for an AI base whose production would empty it is unknown.
        // Deferring every turn keeps the base but re-loses the stockpile each turn, so an AI
        // that never re-queues burns minerals indefinitely. Needs a rules decision (refuse
        // the queue entry at size 1 / let the AI abandon / something else), not this stopgap.
        rBase.DeferProductionAbandon();
        std::cout << "  Base '" << rBase.GetName()
                  << "' deferred production that would empty the base (AI)\n";
        return StageResult_t::Continue;
    }

    EnqueueForPlayer_(rGameState,
                      ProductionAbandonInteraction_t{rFaction.GetFactionId(), rBase.GetBaseId()});
    m_awaitingBaseId = rBase.GetBaseId();
    std::cout << "  Base '" << rBase.GetName()
              << "' awaiting abandon confirmation for production\n";
    return StageResult_t::Yield;
}

StageResult_t BaseProduction::HandleProductionCompleted_(GameState& rGameState, Faction& rFaction,
                                                        BaseManager& rBase,
                                                        const ProductionApplyResult_t& rResult)
{
    if (!rFaction.IsPlayerControlled())
    {
        return StageResult_t::Continue;
    }

    // Queue empty after completion → one popup: what finished, Continue or Zoom to base.
    if (rBase.GetProduction().HasProduction())
    {
        return StageResult_t::Continue;
    }

    EnqueueForPlayer_(
        rGameState,
        ProductionIdleInteraction_t{
            rFaction.GetFactionId(),
            rBase.GetBaseId(),
            true,
            rResult.completedId,
            rResult.completedName.empty() ? rResult.completedId : rResult.completedName,
            rResult.completedEvent,
        });
    m_awaitingBaseId = rBase.GetBaseId();
    return StageResult_t::Yield;
}

void BaseProduction::LogProductionTick_(const BaseManager& rBase,
                                        const ProductionApplyResult_t& rResult)
{
    if (rResult.kind == ProductionApplyKind_t::Completed)
    {
        std::cout << "  Base '" << rBase.GetName() << "' completed production: "
                  << rResult.completedId << "\n";
        return;
    }
    if (rResult.kind != ProductionApplyKind_t::InProgress)
    {
        return;
    }
    const ProductionManager& rProduction = rBase.GetProduction();
    if (const IConstructable* pItem = rProduction.GetCurrentProduction())
    {
        std::cout << "  Base '" << rBase.GetName() << "' producing '" << pItem->GetName()
                  << "' (" << rProduction.GetMineralStockpile() << "/" << rBase.GetMineralCost()
                  << " minerals)\n";
    }
}

StageResult_t BaseProduction::ProcessBase_(GameState& rGameState, Faction& rFaction,
                                           BaseManager& rBase)
{
    const ProductionApplyResult_t result = rBase.ApplyProduction();
    LogProductionTick_(rBase, result);

    switch (result.kind)
    {
    case ProductionApplyKind_t::AwaitingAbandonConfirm:
        return HandleAbandonConfirm_(rGameState, rFaction, rBase);
    case ProductionApplyKind_t::Completed:
        return HandleProductionCompleted_(rGameState, rFaction, rBase, result);
    case ProductionApplyKind_t::Idle:
    case ProductionApplyKind_t::InProgress:
        break;
    }
    return StageResult_t::Continue;
}

StageResult_t BaseProduction::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    EnsureActiveFaction_(rFaction);

    std::cout << "Executing BaseProduction stage for faction\n";

    // Any queue item — this stage's prompt, AI news, an earlier stage's notice — holds the
    // pass here. Resume is prompt-agnostic: once the queue drains, the base that was waiting
    // is done for this pass (razed bases simply never match an id again).
    if (PlayerHasPending_(rGameState))
    {
        return StageResult_t::Yield;
    }
    if (m_awaitingBaseId.has_value())
    {
        m_processedBaseIds.insert(*m_awaitingBaseId);
        m_awaitingBaseId.reset();
    }

    for (BaseManager& rBase : rFaction.Bases())
    {
        const BaseId_t baseId = rBase.GetBaseId();
        if (m_processedBaseIds.contains(baseId))
        {
            continue;
        }

        if (ProcessBase_(rGameState, rFaction, rBase) == StageResult_t::Yield)
        {
            return StageResult_t::Yield;
        }
        m_processedBaseIds.insert(baseId);
    }

    ResetPassState_();
    return StageResult_t::Continue;
}

} // namespace ac

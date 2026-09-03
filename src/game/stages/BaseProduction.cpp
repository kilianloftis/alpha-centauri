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
        // Deferring keeps the base and preserves the stockpile, so the same ready item will
        // re-prompt every turn until the AI re-queues or grows. Needs a rules decision
        // (refuse the queue entry at size 1 / let the AI abandon / something else), not this
        // stopgap.
        rBase.DeferProductionAbandon();
        std::cout << "  Base '" << rBase.GetName()
                  << "' deferred production that would empty the base (AI)\n";
        return StageResult_t::Continue;
    }

    EnqueueForPlayer(rGameState,
                      ProductionAbandonInteraction_t{rFaction.GetFactionId(), rBase.GetBaseId()});
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

    // Production finished — Stockpile Energy is already queued as the fallback. Still offer
    // Continue / Zoom so the player can pick the next real item.
    EnqueueForPlayer(
        rGameState,
        ProductionIdleInteraction_t{
            rFaction.GetFactionId(),
            rBase.GetBaseId(),
            true,
            rResult.completedId,
            rResult.completedName.empty() ? rResult.completedId : rResult.completedName,
            rResult.completedEvent,
        });
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
        if (pItem->NeverCompletes())
        {
            std::cout << "  Base '" << rBase.GetName() << "' stockpiling '" << pItem->GetName()
                      << "'\n";
            return;
        }
        std::cout << "  Base '" << rBase.GetName() << "' producing '" << pItem->GetName()
                  << "' (" << rProduction.GetMineralStockpile() << "/" << rBase.GetMineralCost()
                  << " minerals)\n";
    }
}

StageResult_t BaseProduction::HandleApplyResult_(GameState& rGameState, Faction& rFaction,
                                                BaseManager& rBase,
                                                const ProductionApplyResult_t& rResult)
{
    switch (rResult.kind)
    {
    case ProductionApplyKind_t::AwaitingAbandonConfirm:
        return HandleAbandonConfirm_(rGameState, rFaction, rBase);
    case ProductionApplyKind_t::Completed:
        return HandleProductionCompleted_(rGameState, rFaction, rBase, rResult);
    case ProductionApplyKind_t::Idle:
    case ProductionApplyKind_t::InProgress:
        break;
    }
    return StageResult_t::Continue;
}

StageResult_t BaseProduction::ReevaluateProcessedBases_(GameState& rGameState, Faction& rFaction)
{
    bool bProgress = true;
    while (bProgress)
    {
        bProgress = false;
        for (BaseManager& rBase : rFaction.Bases())
        {
            if (!m_processedBaseIds.contains(rBase.GetBaseId()))
            {
                continue;
            }
            if (rBase.HasPendingProductionAbandonConfirm())
            {
                continue;
            }

            const ProductionApplyResult_t result = rBase.TryCompleteReadyProduction();
            if (result.kind != ProductionApplyKind_t::Completed
                && result.kind != ProductionApplyKind_t::AwaitingAbandonConfirm)
            {
                continue;
            }

            LogProductionTick_(rBase, result);
            if (result.kind == ProductionApplyKind_t::Completed)
            {
                bProgress = true;
            }
            if (HandleApplyResult_(rGameState, rFaction, rBase, result) == StageResult_t::Yield)
            {
                return StageResult_t::Yield;
            }
        }
    }
    return StageResult_t::Continue;
}

StageResult_t BaseProduction::ProcessBase_(GameState& rGameState, Faction& rFaction,
                                           BaseManager& rBase)
{
    const ProductionApplyResult_t result = rBase.ApplyProduction();
    LogProductionTick_(rBase, result);

    const StageResult_t applyResult = HandleApplyResult_(rGameState, rFaction, rBase, result);
    if (result.kind != ProductionApplyKind_t::Completed)
    {
        return applyResult;
    }

    // The completed item may have been a prototype, which drops the surcharge on every other
    // queue this faction owns. Bases already visited this pass get a second look.
    if (ReevaluateProcessedBases_(rGameState, rFaction) == StageResult_t::Yield)
    {
        return StageResult_t::Yield;
    }
    return applyResult;
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

    if (ReevaluateProcessedBases_(rGameState, rFaction) == StageResult_t::Yield)
    {
        return StageResult_t::Yield;
    }

    for (BaseManager& rBase : rFaction.Bases())
    {
        const BaseId_t baseId = rBase.GetBaseId();
        if (m_processedBaseIds.contains(baseId))
        {
            continue;
        }

        // Marked before the call, not after: ProcessBase_ can yield from any of several
        // nested paths, and a base that has already had ApplyProduction run
        // must not be revisited when the pass resumes.
        m_processedBaseIds.insert(baseId);
        if (ProcessBase_(rGameState, rFaction, rBase) == StageResult_t::Yield)
        {
            return StageResult_t::Yield;
        }
    }

    ResetPassState_();
    return StageResult_t::Continue;
}

} // namespace ac

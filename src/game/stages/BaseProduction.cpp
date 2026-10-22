#include "game/stages/BaseProduction.h"

#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/production/IConstructable.h"
#include "game/PlayerInteraction.h"
#include "game/PlayerInteractionQueue.h"
#include "game/base/BaseManager.h"
#include "game/production/ProductionManager.h"
#include "game/stages/TurnStageRegistrar.h"

#include <iostream>
#include <optional>
#include <utility>

namespace ac
{

namespace { TurnStageRegistrar<BaseProduction> g_registrar("BaseProduction"); }

BaseProduction::BaseProduction(HookContext hookContext)
    : YieldingPerFactionTurnStage(std::move(hookContext))
{
}

ProductionApplyResult_t BaseProduction::TickBase_(BaseManager& rBase)
{
    rBase.ApplyProduction();
    ProductionApplyResult_t result = rBase.GetProduction().TryCompleteReadyAtTurnStart();
    return ValidateQueueAfter_(rBase, std::move(result));
}

ProductionApplyResult_t BaseProduction::ValidateQueueAfter_(BaseManager& rBase,
                                                            ProductionApplyResult_t result)
{
    std::optional<std::string> droppedCurrent = rBase.GetProduction().ValidateQueue();
    if (droppedCurrent)
    {
        result.preempted = ProductionPreempt_t{std::move(*droppedCurrent)};
    }
    return result;
}

void BaseProduction::OnResetPassState_()
{
    m_processedBaseIds.clear();
}

void BaseProduction::OnExitImpl()
{
    ResetPassState_();
}

StageResult_t BaseProduction::HandlePreempted_(GameState& rGameState, Faction& rFaction,
                                              BaseManager& rBase,
                                              const ProductionPreempt_t& rPreempted)
{
    if (!rFaction.IsPlayerControlled())
    {
        return StageResult_t::Continue;
    }

    EnqueueForPlayer(rGameState,
                     ProductionPreemptedInteraction_t{
                         rFaction.GetFactionId(), rBase.GetBaseId(), rPreempted.droppedName});
    return StageResult_t::Yield;
}

StageResult_t BaseProduction::HandleAwaitingConfirmation_(GameState& rGameState,
                                                         Faction& rFaction, BaseManager& rBase)
{
    if (!rFaction.IsPlayerControlled())
    {
        // The AI answers "not this turn", as a player can; the item stays funded
        // and is asked again, so a growing base completes it.
        rBase.DeferProductionCompletion();
        std::cout << "  Base '" << rBase.GetName()
                  << "' deferred production that would abandon the base (AI)\n";
        return StageResult_t::Continue;
    }

    EnqueueForPlayer(rGameState,
                      ProductionWouldEmptyInteraction_t{rFaction.GetFactionId(), rBase.GetBaseId()});
    std::cout << "  Base '" << rBase.GetName()
              << "' awaiting choice for production that would empty the base\n";
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

    EnqueueForPlayer(
        rGameState,
        ProductionIdleInteraction_t{
            rFaction.GetFactionId(),
            rBase.GetBaseId(),
            true,
            rResult.completedId,
            rResult.completedName.empty() ? rResult.completedId : rResult.completedName,
            rResult.completedEvents,
        });
    return StageResult_t::Yield;
}

void BaseProduction::LogProductionTick_(const BaseManager& rBase,
                                        const ProductionApplyResult_t& rResult)
{
    if (rResult.preempted)
    {
        std::cout << "  Base '" << rBase.GetName() << "' can no longer produce "
                  << rResult.preempted->droppedName << "\n";
        return;
    }
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
        if (pItem->IsStockpile())
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
    if (rResult.preempted)
    {
        return HandlePreempted_(rGameState, rFaction, rBase, *rResult.preempted);
    }

    switch (rResult.kind)
    {
    case ProductionApplyKind_t::AwaitingConfirmation:
        return HandleAwaitingConfirmation_(rGameState, rFaction, rBase);
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
            if (rBase.HasPendingProductionConfirmation())
            {
                continue;
            }

            const ProductionApplyResult_t result =
                ValidateQueueAfter_(rBase, rBase.GetProduction().TryCompleteReady());
            if (!result.preempted && result.kind != ProductionApplyKind_t::Completed
                && result.kind != ProductionApplyKind_t::AwaitingConfirmation)
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
    const ProductionApplyResult_t result = TickBase_(rBase);
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

    // Any queued interaction holds the pass here; once the queue drains, the waiting base is done
    // for this pass.
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

        // Before the call: ProcessBase_ can yield from several paths, and a ticked base must not be
        // revisited on resume.
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

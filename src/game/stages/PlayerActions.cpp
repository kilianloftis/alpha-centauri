#include "game/stages/PlayerActions.h"
#include "game/GameState.h"
#include "game/faction/Faction.h"
#include "game/units/UnitManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/stages/TurnStageRegistrar.h"

#include <variant>

namespace ac
{

namespace { TurnStageRegistrar<PlayerActions> g_registrar("PlayerActions"); }

PlayerActions::PlayerActions(HookContext hookContext)
    : YieldingPerFactionTurnStage(std::move(hookContext))
{
}

void PlayerActions::OnResetPassState_()
{
    m_phase = Phase_t::AwaitingInteraction;
    m_advancedUnitIds.clear();
}

void PlayerActions::OnExitImpl()
{
    ResetPassState_();
}

StageResult_t PlayerActions::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    EnsureActiveFaction_(rFaction);

    const bool bPlayer = rFaction.IsPlayerControlled();

    // Queued interactions come first, in either phase: the player answers them before being
    // handed the orders phase, and before any order resolves on the way out.
    if (bPlayer && PlayerHasPending_(rGameState))
    {
        return StageResult_t::Yield;
    }

    if (bPlayer && m_phase == Phase_t::AwaitingInteraction)
    {
        m_phase = Phase_t::EndingInteraction;
        return StageResult_t::Yield;
    }

    UnitOrderExecutor& rExecutor = rGameState.GetUnitOrderExecutor();
    UnitManager& rUnitManager = rFaction.GetUnitManager();

    // Execute may report Expended (SingleUse). DestroyUnit under deferral so the current
    // unit stays safe to reference until this pass reaches a safe point.
    const auto destructionScope = rUnitManager.DeferDestruction();
    for (Unit& rUnit : rUnitManager.Units())
    {
        if (!ShouldAdvance_(rUnit))
        {
            continue;
        }
        const OrderProgress_t progress = AdvanceUnit_(rUnit, rExecutor, rUnitManager);
        // Already destroyed while advancing (e.g. a native raider consumed by its raid).
        const bool bGone = progress == OrderProgress_t::UnitDestroyed
                           || progress == OrderProgress_t::Expended;
        if (!bGone && bPlayer && rUnit.RequiresOrders())
        {
            return StageResult_t::Yield;
        }
    }

    ResetPassState_();
    return StageResult_t::Continue;
}

bool PlayerActions::ShouldAdvance_(const Unit& rUnit) const
{
    if (m_advancedUnitIds.contains(rUnit.GetUnitId()) || !rUnit.GetOrder().has_value())
    {
        return false;
    }
    return !std::holds_alternative<MoveOrder_t>(*rUnit.GetOrder())
        || rUnit.GetMoveFragmentsRemaining() > 0;
}

OrderProgress_t PlayerActions::AdvanceUnit_(Unit& rUnit, UnitOrderExecutor& rExecutor,
                                            UnitManager& rUnitManager)
{
    const OrderProgress_t progress = rExecutor.Execute(rUnit);
    if (progress == OrderProgress_t::Continue)
    {
        // Multi-turn order still in progress — do not re-tick on mid-pass Yield resume.
        m_advancedUnitIds.insert(rUnit.GetUnitId());
    }
    if (progress == OrderProgress_t::Expended)
    {
        rUnitManager.DestroyUnit(rUnit);
    }
    return progress;
}

} // namespace ac

#include "game/stages/PlayerActions.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/UnitManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/TurnStageRegistrar.h"

#include <variant>

namespace ac
{

namespace { TurnStageRegistrar<PlayerActions> g_registrar("PlayerActions"); }

PlayerActions::PlayerActions(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

bool PlayerActions::DoesUnitRequireOrders_(const Unit& rUnit)
{
    return !rUnit.GetOrder().has_value() && rUnit.GetMoveFragmentsRemaining() > 0;
}

StageResult_t PlayerActions::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    const bool bPlayer = rFaction.IsPlayerControlled();

    if (bPlayer && m_phase == Phase_t::AwaitingInteraction)
    {
        m_phase = Phase_t::EndingInteraction;
        return StageResult_t::Yield;
    }

    UnitOrderExecutor& rExecutor = rGameState.GetUnitOrderExecutor();

    for (Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        if (!rUnit.GetOrder().has_value())
        {
            continue;
        }

        if (std::holds_alternative<MoveOrder_t>(*rUnit.GetOrder())
            && rUnit.GetMoveFragmentsRemaining() <= 0)
        {
            continue;
        }

        rExecutor.Execute(rUnit);

        if (bPlayer && DoesUnitRequireOrders_(rUnit))
        {
            return StageResult_t::Yield;
        }
    }

    if (bPlayer)
    {
        m_phase = Phase_t::AwaitingInteraction;
    }
    return StageResult_t::Continue;
}

} // namespace ac

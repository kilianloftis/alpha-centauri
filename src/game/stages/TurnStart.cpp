#include "game/stages/TurnStart.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include "game/faction/UnitManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrderExecutor.h"
#include "lib/EventBus.h"
#include "lib/GameEvent.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<TurnStart> g_registrar("TurnStart"); }

TurnStart::TurnStart(HookContext hookContext)
    : GlobalTurnStage(std::move(hookContext))
{
}

StageResult_t TurnStart::ExecuteImpl(GameState& rGameState)
{
    rGameState.IncrementMissionYear();
    rGameState.GetEventBus().Publish(EvTurnStarted{ rGameState.GetMissionYear() });

    std::cout << "\n--- Mission Year " << rGameState.GetMissionYear() << " ---\n";

    UnitOrderExecutor& rOrders = rGameState.GetUnitOrderExecutor();
    for (Faction& rFaction : rGameState.Factions())
    {
        for (Unit& rUnit : rFaction.GetUnitManager().Units())
        {
            rUnit.BeginTurn();
            rOrders.OnTurnStart(rUnit);
        }
    }
    return StageResult_t::Continue;
}

} // namespace ac

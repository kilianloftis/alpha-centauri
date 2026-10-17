#include "game/stages/TurnEnd.h"
#include "game/GameState.h"
#include "game/stages/TurnStageRegistrar.h"
#include "game/movement/FuelRules.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<TurnEnd> g_registrar("TurnEnd"); }

TurnEnd::TurnEnd(HookContext hookContext)
    : GlobalTurnStage(std::move(hookContext))
{
}

StageResult_t TurnEnd::ExecuteImpl(GameState& rGameState)
{
    std::cout << "Executing TurnEnd stage\n";
    ProcessAllFuelAtTurnEnd(rGameState);
    return StageResult_t::Continue;
}

} // namespace ac

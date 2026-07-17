#include "game/stages/VictoryConditionChecks.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<VictoryConditionChecks> g_registrar("VictoryConditionChecks"); }

VictoryConditionChecks::VictoryConditionChecks(HookContext hookContext)
    : GlobalTurnStage(std::move(hookContext))
{
}

StageResult_t VictoryConditionChecks::ExecuteImpl(GameState& rGameState)
{
    (void)rGameState;
    std::cout << "Executing VictoryConditionChecks stage\n";
    return StageResult_t::Continue;
}

} // namespace ac

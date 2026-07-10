#include "game/stages/VictoryConditionChecks.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<VictoryConditionChecks> g_registrar("VictoryConditionChecks"); }

VictoryConditionChecks::VictoryConditionChecks(std::shared_ptr<HookContext> pHookContext)
    : GlobalTurnStage(pHookContext)
{
}

void VictoryConditionChecks::ExecuteImpl(GameState& rGameState)
{
    (void)rGameState;
    std::cout << "Executing VictoryConditionChecks stage\n";
}

} // namespace ac

#include "game/stages/VictoryConditionChecks.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

VictoryConditionChecks::VictoryConditionChecks(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void VictoryConditionChecks::Execute_(GameState* pGameState)
{
    (void)pGameState;
    std::cout << "Executing VictoryConditionChecks stage\n";
}

} // namespace ac

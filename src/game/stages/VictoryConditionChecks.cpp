#include "game/stages/VictoryConditionChecks.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

VictoryConditionChecks::VictoryConditionChecks(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void VictoryConditionChecks::Execute_(GameState* pGameState, Faction* pFaction)
{
    (void)pGameState;
    (void)pFaction;
    std::cout << "Executing VictoryConditionChecks stage\n";
}

} // namespace ac

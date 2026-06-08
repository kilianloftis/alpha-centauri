#include "game/stages/PlayerActions.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

PlayerActions::PlayerActions(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void PlayerActions::Execute_(GameState* pGameState)
{
    (void)pGameState;
    std::cout << "Executing PlayerActions stage\n";
}

} // namespace ac

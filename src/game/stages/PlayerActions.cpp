#include "game/stages/PlayerActions.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

PlayerActions::PlayerActions(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void PlayerActions::Execute_(GameState* pGameState, Faction* pFaction)
{
    (void)pGameState;
    (void)pFaction;
    std::cout << "Executing PlayerActions stage\n";
}

} // namespace ac

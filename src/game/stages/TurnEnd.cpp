#include "game/stages/TurnEnd.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

TurnEnd::TurnEnd(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void TurnEnd::Execute_(GameState* pGameState)
{
    (void)pGameState;
    std::cout << "Executing TurnEnd stage\n";
}

} // namespace ac

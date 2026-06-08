#include "game/stages/TurnStart.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

TurnStart::TurnStart(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void TurnStart::Execute_(GameState* pGameState)
{
    (void)pGameState;
    std::cout << "Executing TurnStart stage\n";
}

} // namespace ac

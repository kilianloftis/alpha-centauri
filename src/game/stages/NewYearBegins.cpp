#include "game/stages/NewYearBegins.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

NewYearBegins::NewYearBegins(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void NewYearBegins::Execute_(GameState* pGameState)
{
    (void)pGameState;
    std::cout << "Executing NewYearBegins stage\n";
}

} // namespace ac

#include "game/stages/NewYearBegins.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

NewYearBegins::NewYearBegins(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void NewYearBegins::Execute_(GameState* pGameState, Faction* pFaction)
{
    (void)pGameState;
    (void)pFaction;
    std::cout << "Executing NewYearBegins stage\n";
}

} // namespace ac

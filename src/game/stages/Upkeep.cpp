#include "game/stages/Upkeep.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

Upkeep::Upkeep(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void Upkeep::Execute_(GameState* pGameState, Faction* pFaction)
{
    (void)pGameState;
    (void)pFaction;
    std::cout << "Executing Upkeep stage\n";
}

} // namespace ac

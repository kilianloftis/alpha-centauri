#include "game/stages/BaseProduction.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

BaseProduction::BaseProduction(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void BaseProduction::Execute_(GameState* pGameState, Faction* pFaction)
{
    (void)pGameState;
    (void)pFaction;
    std::cout << "Executing BaseProduction stage\n";
}

} // namespace ac

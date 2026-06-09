#include "game/stages/Save.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

Save::Save(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void Save::Execute_(GameState* pGameState, Faction* pFaction)
{
    (void)pGameState;
    (void)pFaction;
    std::cout << "Executing Save stage\n";
}

} // namespace ac

#include "game/stages/WorldEvents.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

WorldEvents::WorldEvents(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void WorldEvents::Execute_(GameState* pGameState, Faction* pFaction)
{
    (void)pGameState;
    (void)pFaction;
    std::cout << "Executing WorldEvents stage\n";
}

} // namespace ac

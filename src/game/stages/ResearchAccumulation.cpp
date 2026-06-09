#include "game/stages/ResearchAccumulation.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

ResearchAccumulation::ResearchAccumulation(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void ResearchAccumulation::Execute_(GameState* pGameState, Faction* pFaction)
{
    (void)pGameState;
    (void)pFaction;
    std::cout << "Executing ResearchAccumulation stage\n";
}

} // namespace ac

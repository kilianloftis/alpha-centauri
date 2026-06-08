#include "game/stages/EnergyAllocation.h"
#include "game/GameState.h"
#include <iostream>

namespace ac
{

EnergyAllocation::EnergyAllocation(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void EnergyAllocation::Execute_(GameState* pGameState)
{
    (void)pGameState;
    std::cout << "Executing EnergyAllocation stage\n";
}

} // namespace ac

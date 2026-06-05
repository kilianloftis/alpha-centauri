#include "game/stages/EnergyAllocation.h"
#include <iostream>

namespace ac
{

EnergyAllocation::EnergyAllocation(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void EnergyAllocation::Execute_()
{
    std::cout << "Executing EnergyAllocation stage\n";
}

} // namespace ac

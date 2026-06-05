#include "game/stages/ResearchAccumulation.h"
#include <iostream>

namespace ac
{

ResearchAccumulation::ResearchAccumulation(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void ResearchAccumulation::Execute_()
{
    std::cout << "Executing ResearchAccumulation stage\n";
}

} // namespace ac

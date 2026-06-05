#include "game/stages/Population.h"
#include <iostream>

namespace ac
{

Population::Population(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void Population::Execute_()
{
    std::cout << "Executing Population stage\n";
}

} // namespace ac

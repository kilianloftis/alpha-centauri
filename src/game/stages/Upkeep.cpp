#include "game/stages/Upkeep.h"
#include <iostream>

namespace ac
{

Upkeep::Upkeep(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void Upkeep::Execute_()
{
    std::cout << "Executing Upkeep stage\n";
}

} // namespace ac

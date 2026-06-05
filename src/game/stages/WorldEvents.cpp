#include "game/stages/WorldEvents.h"
#include <iostream>

namespace ac
{

WorldEvents::WorldEvents(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void WorldEvents::Execute_()
{
    std::cout << "Executing WorldEvents stage\n";
}

} // namespace ac

#include "game/stages/TurnStart.h"
#include <iostream>

namespace ac
{

TurnStart::TurnStart(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void TurnStart::Execute_()
{
    std::cout << "Executing TurnStart stage\n";
}

} // namespace ac

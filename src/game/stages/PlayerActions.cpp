#include "game/stages/PlayerActions.h"
#include <iostream>

namespace ac
{

PlayerActions::PlayerActions(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void PlayerActions::Execute_()
{
    std::cout << "Executing PlayerActions stage\n";
}

} // namespace ac

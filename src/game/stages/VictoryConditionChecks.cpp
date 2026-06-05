#include "game/stages/VictoryConditionChecks.h"
#include <iostream>

namespace ac
{

VictoryConditionChecks::VictoryConditionChecks(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void VictoryConditionChecks::Execute_()
{
    std::cout << "Executing VictoryConditionChecks stage\n";
}

} // namespace ac

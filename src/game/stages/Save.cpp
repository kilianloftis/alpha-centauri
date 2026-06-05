#include "game/stages/Save.h"
#include <iostream>

namespace ac
{

Save::Save(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void Save::Execute_()
{
    std::cout << "Executing Save stage\n";
}

} // namespace ac

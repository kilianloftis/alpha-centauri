#include "game/stages/NewYearBegins.h"
#include <iostream>

namespace ac
{

NewYearBegins::NewYearBegins(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void NewYearBegins::Execute_()
{
    std::cout << "Executing NewYearBegins stage\n";
}

} // namespace ac

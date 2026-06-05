#include "game/stages/IncomeCollection.h"
#include <iostream>

namespace ac
{

IncomeCollection::IncomeCollection(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void IncomeCollection::Execute_()
{
    std::cout << "Executing IncomeCollection stage\n";
}

} // namespace ac

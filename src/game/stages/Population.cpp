#include "game/stages/Population.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include <iostream>

namespace ac
{

Population::Population(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void Population::Execute_(GameState* pGameState, Faction* pFaction)
{
    std::cout << "Executing Population stage\n";

    (void)pGameState;

    if (!pFaction)
    {
        return;
    }

    for (size_t i = 0; i < pFaction->GetBaseCount(); ++i)
    {
        BaseManager* pBase = pFaction->GetBase(i);
        if (!pBase)
        {
            continue;
        }

        std::cout << "  Accumulating growth for base '" << pBase->GetName() << "' (bank: " << pBase->GetNutrientStockpile() << " -> ";

        pBase->ApplyGrowth();

        std::cout << pBase->GetNutrientStockpile() << ", size: " << pBase->GetBaseSize() << ")\n";

        pBase->RecalculatePopComposition();
    }
}

} // namespace ac

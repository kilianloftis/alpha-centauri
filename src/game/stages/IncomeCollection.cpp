#include "game/stages/IncomeCollection.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include <iostream>

namespace ac
{

IncomeCollection::IncomeCollection(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void IncomeCollection::Execute_(GameState* pGameState, Faction* pFaction)
{
    (void)pGameState;

    if (!pFaction)
    {
        std::cout << "Executing IncomeCollection stage (no faction)\n";
        return;
    }

    std::cout << "Executing IncomeCollection stage for faction\n";

    int totalIncome = 0;

    for (BaseManager& rBase : pFaction->Bases())
    {
        int baseIncome = rBase.ConsumeEcon();
        totalIncome += baseIncome;

        std::cout << "  Base '" << rBase.GetName() << "' income: " << baseIncome << "\n";
    }

    pFaction->AddEnergy(totalIncome);

    std::cout << "  Faction total energy: " << pFaction->GetEnergy() << "\n";
}

} // namespace ac

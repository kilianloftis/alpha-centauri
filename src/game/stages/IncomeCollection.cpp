#include "game/stages/IncomeCollection.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/EconomyManager.h"
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

    const int totalIncome = pFaction->CollectIncome();

    std::cout << "  Faction income: " << totalIncome
              << ", total energy: " << pFaction->GetEconomy().GetEnergy() << "\n";
}

} // namespace ac

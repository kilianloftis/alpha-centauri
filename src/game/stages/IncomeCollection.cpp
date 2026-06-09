#include "game/stages/IncomeCollection.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/Economy.h"
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

    // Energy allocated to Econ is set by the EnergyAllocation stage.
    // We add that allocated amount to the faction's energy reserve.
    if (Economy* pEconomy = pFaction->GetEconomy())
    {
        int econEnergy = pEconomy->GetEnergyForEcon();
        pEconomy->AddEnergyReserve(econEnergy);
        std::cout << "  Energy allocated to Econ: " << econEnergy
                  << " (added to faction reserve)\n";
        std::cout << "  Current faction energy reserve: " << pEconomy->GetCurrentEnergyReserve() << "\n";
    }
}

} // namespace ac

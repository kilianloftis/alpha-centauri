#include "game/stages/EnergyAllocation.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/base/resources/BaseEconomyManager.h"
#include <iostream>

namespace ac
{

EnergyAllocation::EnergyAllocation(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void EnergyAllocation::Execute_(GameState* pGameState, Faction* pFaction)
{
    (void)pGameState;

    if (!pFaction)
    {
        std::cout << "Executing EnergyAllocation stage (no faction)\n";
        return;
    }

    std::cout << "Executing EnergyAllocation stage for faction\n";

    // Calculate total energy production from all bases
    int totalEnergy = 0;
    for (size_t i = 0; i < pFaction->GetBaseCount(); ++i)
    {
        BaseManager* pBase = pFaction->GetBase(i);
        if (pBase)
        {
            ResourceManager* pResources = pBase->GetResourceManager();
            if (pResources)
            {
                totalEnergy += pResources->GetEnergyProduction();
            }
        }
    }

    std::cout << "  Total energy produced: " << totalEnergy << "\n";

    // Set up default allocation: 40% Econ, 50% Labs, 10% Psych
    if (BaseEconomyManager* pEconomy = pFaction->GetEconomy())
    {
        EnergyAllocation_t allocation;
        allocation.econPercent = 40;
        allocation.labsPercent = 50;
        allocation.psychPercent = 10;

        pEconomy->SetEnergyAllocation(allocation);
        pEconomy->SetTotalEnergyProduced(totalEnergy);

        std::cout << "  Energy allocated: "
                  << pEconomy->GetEnergyForEcon() << " to Econ (" << allocation.econPercent << "%), "
                  << pEconomy->GetEnergyForLabs() << " to Labs (" << allocation.labsPercent << "%), "
                  << pEconomy->GetEnergyForPsych() << " to Psych (" << allocation.psychPercent << "%)\n";
    }
}

} // namespace ac

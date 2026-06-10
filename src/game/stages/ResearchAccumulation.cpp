#include "game/stages/ResearchAccumulation.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/resources/BaseEconomyManager.h"
#include "game/faction/Research.h"
#include <iostream>

namespace ac
{

ResearchAccumulation::ResearchAccumulation(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void ResearchAccumulation::Execute_(GameState* pGameState, Faction* pFaction)
{
    (void)pGameState;

    if (!pFaction)
    {
        std::cout << "Executing ResearchAccumulation stage (no faction)\n";
        return;
    }

    std::cout << "Executing ResearchAccumulation stage for faction\n";

    // Energy allocated to Labs is set by the EnergyAllocation stage.
    // We convert that energy to research points.
    if (BaseEconomyManager* pEconomy = pFaction->GetEconomy())
    {
        int labsEnergy = pEconomy->GetEnergyForLabs();

        if (Research* pResearch = pFaction->GetResearch())
        {
            // TODO: Apply any modifiers (e.g., from facilities, techs, etc.)
            int researchPoints = labsEnergy;
            pResearch->AddResearchPoints(researchPoints);

            std::cout << "  Energy allocated to Labs: " << labsEnergy
                      << " -> " << researchPoints << " research points\n";
        }
    }
}

} // namespace ac

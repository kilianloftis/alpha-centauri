#include "game/stages/ResearchAccumulation.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseManager.h"
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

    int totalLabs = 0;

    for (BaseManager& rBase : pFaction->Bases())
    {
        int baseLabs = rBase.ConsumeLabs();
        totalLabs += baseLabs;

        std::cout << "  Base '" << rBase.GetName() << "' labs: " << baseLabs << "\n";
    }

    pFaction->AddResearchPoints(totalLabs);

    std::cout << "  Faction total research points: " << pFaction->GetResearchPoints() << "\n";

    ResearchManager* pResearch = pFaction->GetResearchManager();
    while (pResearch && pResearch->CanDiscoverTech())
    {
        const TechId techId = pResearch->GetResearchTarget();
        if (!pFaction->DiscoverCurrentResearch())
        {
            break;
        }

        std::cout << "  Discovered tech: '" << techId << "'\n";
    }
}

} // namespace ac

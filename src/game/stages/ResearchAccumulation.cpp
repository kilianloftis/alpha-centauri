#include "game/stages/ResearchAccumulation.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/ResearchManager.h"
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

    const int totalLabs = pFaction->CollectResearch();

    ResearchManager& rResearch = pFaction->GetResearch();
    std::cout << "  Faction labs collected: " << totalLabs
              << ", total research points: " << rResearch.GetAccumulatedPoints() << "\n";

    while (rResearch.CanDiscoverTech())
    {
        const TechId techId = rResearch.GetResearchTarget();
        if (!pFaction->DiscoverCurrentResearch())
        {
            break;
        }

        std::cout << "  Discovered tech: '" << techId << "'\n";
    }
}

} // namespace ac

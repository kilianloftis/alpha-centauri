#include "game/stages/ResearchAccumulation.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/ResearchManager.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<ResearchAccumulation> g_registrar("ResearchAccumulation"); }

ResearchAccumulation::ResearchAccumulation(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t ResearchAccumulation::ExecuteImpl(GameState& /*rGameState*/, Faction& rFaction)
{
    std::cout << "Executing ResearchAccumulation stage for faction\n";

    const int totalLabs = rFaction.CollectResearch();

    ResearchManager& rResearch = rFaction.GetResearch();
    std::cout << "  Faction labs collected: " << totalLabs
              << ", total research points: " << rResearch.GetAccumulatedPoints() << "\n";

    while (rResearch.CanDiscoverTech())
    {
        const TechId techId = rResearch.GetResearchTarget();
        if (!rFaction.DiscoverCurrentResearch())
        {
            break;
        }

        std::cout << "  Discovered tech: '" << techId << "'\n";
    }
    return StageResult_t::Continue;
}

} // namespace ac

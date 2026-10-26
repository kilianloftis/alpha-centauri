#include "game/stages/ResearchAccumulation.h"
#include "game/GameState.h"
#include "game/faction/Faction.h"
#include "game/research/ResearchManager.h"
#include "game/stages/TurnStageRegistrar.h"
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

    const ResearchManager& rResearch = rFaction.GetResearch();
    const std::size_t knownBefore = rResearch.GetDiscoveredTechs().size();
    const int totalLabs = rFaction.CollectResearch();
    std::cout << "  Faction labs collected: " << totalLabs
              << ", total research points: " << rResearch.GetAccumulatedPoints() << "\n";

    const std::vector<TechId>& rKnown = rResearch.GetDiscoveredTechs();
    for (std::size_t i = knownBefore; i < rKnown.size(); ++i)
    {
        std::cout << "  Discovered tech: '" << rKnown[i] << "'\n";
    }
    return StageResult_t::Continue;
}

} // namespace ac

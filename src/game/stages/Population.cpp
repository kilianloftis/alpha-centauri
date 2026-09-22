#include "game/stages/Population.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"

namespace ac
{

namespace { TurnStageRegistrar<Population> g_registrar("Population"); }

Population::Population(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t Population::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    (void)rGameState;
    // Growth and starvation are the BaseGrowth stage (after BaseProduction). What is left here
    // is the composition and mood pass, which stays after production because a completed
    // facility can change either.
    for (BaseManager& rBase : rFaction.Bases())
    {
        ProcessBase_(rBase);
    }
    return StageResult_t::Continue;
}

void Population::ProcessBase_(BaseManager& rBase)
{
    PopulationManager& rPopulation = rBase.GetPopulation();
    rPopulation.AdvanceAssimilation();
    rPopulation.EnsureCompositionCurrent();
    rPopulation.ForecastMood();
}

} // namespace ac

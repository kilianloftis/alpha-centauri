#include "game/stages/Population.h"
#include "game/faction/Faction.h"
#include "game/base/BaseManager.h"
#include "game/population/PopulationManager.h"
#include "game/GameState.h"
#include "game/stages/TurnStageRegistrar.h"

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
    // Composition and mood, after production, since a completed facility can change either.
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

#include "game/stages/Population.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/TurnStageRegistrar.h"

namespace ac
{

namespace { TurnStageRegistrar<Population> g_registrar("Population"); }

Population::Population(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t Population::ExecuteImpl(GameState& /*rGameState*/, Faction& rFaction)
{
    // World/council extras are already in Faction::GetActiveEffects via BindWorldEffects.
    rFaction.ApplyBaseGrowth();

    for (BaseManager& rBase : rFaction.Bases())
    {
        PopulationManager& rPopulation = rBase.GetPopulation();
        rPopulation.RecalculateComposition();
        rPopulation.CheckRiotEndOfTurn();
        rPopulation.CheckGoldenAgeEndOfTurn();
    }
    return StageResult_t::Continue;
}

} // namespace ac

#include "game/stages/Population.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"

#include <vector>

namespace ac
{

namespace { TurnStageRegistrar<Population> g_registrar("Population"); }

Population::Population(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t Population::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    // World/council extras are already in Faction::GetActiveEffects via BindWorldEffects.
    rFaction.ApplyBaseGrowth();

    std::vector<BaseId_t> depopulated;
    for (BaseManager& rBase : rFaction.Bases())
    {
        PopulationManager& rPopulation = rBase.GetPopulation();
        if (rPopulation.GetSize() == 0)
        {
            // Collected, not razed in place: RazeBase destroys the BaseManager, which would
            // invalidate this iteration.
            depopulated.push_back(rBase.GetBaseId());
            continue;
        }
        rBase.AdvanceAssimilation();
        rPopulation.RecalculateComposition();
        rPopulation.CheckRiotEndOfTurn();
        rPopulation.CheckGoldenAgeEndOfTurn();
    }

    // A base that starves to nothing is razed, through the same pathway conquest uses.
    for (const BaseId_t baseId : depopulated)
    {
        if (BaseManager* pBase = rFaction.FindBase(baseId))
        {
            rGameState.RazeBase(*pBase);
        }
    }
    return StageResult_t::Continue;
}

} // namespace ac

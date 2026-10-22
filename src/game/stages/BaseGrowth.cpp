#include "game/stages/BaseGrowth.h"
#include "game/faction/Faction.h"
#include "game/base/BaseManager.h"
#include "game/population/PopulationManager.h"
#include "game/stages/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<BaseGrowth> g_registrar("BaseGrowth"); }

BaseGrowth::BaseGrowth(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

// Ordered after BaseProduction, as in SMAC, so a Hab Complex / Dome completing this turn raises
// MaxBaseSize before ApplyGrowth.
StageResult_t BaseGrowth::ExecuteImpl(GameState& /*rGameState*/, Faction& rFaction)
{
    std::cout << "Executing BaseGrowth stage for faction\n";

    rFaction.ApplyBaseGrowth();

    // AddPop leaves the drone/talent split alone, so a base that grew is reconciled here, before
    // later stages read pop effects. RemovePop reconciles itself.
    for (BaseManager& rBase : rFaction.Bases())
    {
        rBase.GetPopulation().EnsureCompositionCurrent();
    }
    return StageResult_t::Continue;
}

} // namespace ac

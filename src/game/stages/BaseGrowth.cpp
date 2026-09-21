#include "game/stages/BaseGrowth.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<BaseGrowth> g_registrar("BaseGrowth"); }

BaseGrowth::BaseGrowth(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

// Ordered after BaseProduction so a Hab Complex / Dome completing this turn raises MaxBaseSize
// before ApplyGrowth. Colony-pod abandon is handled in production via WouldGrowThisTurn and
// CommitPendingGrowth before on-complete pop costs — see docs/game-rules-decisions.md §10.
StageResult_t BaseGrowth::ExecuteImpl(GameState& /*rGameState*/, Faction& rFaction)
{
    std::cout << "Executing BaseGrowth stage for faction\n";

    rFaction.ApplyBaseGrowth();

    // PopulationManager::AddPop deliberately does not reconcile the drone/talent split — the
    // caller names the type it wants, and reconciling inside the add would overwrite it. So a
    // base that grew is left describing the size it no longer has until something asks. Do it
    // here, because later stages read pop-generated effects and must not see a split for the
    // previous size. (RemovePop already reconciles itself, so a base that starved needs nothing.)
    for (BaseManager& rBase : rFaction.Bases())
    {
        rBase.GetPopulation().EnsureCompositionCurrent();
    }
    return StageResult_t::Continue;
}

} // namespace ac

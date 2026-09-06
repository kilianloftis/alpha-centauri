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

// TODO: this stage's position conflicts with a population cap that buildings can raise, which
// PopulationManager::GetMaxSize still has as its own TODO (nothing raises m_maxSize today, so
// the conflict is not yet reachable). Running before BaseProduction is what lets the abandon
// check judge an item's pop cost against the size the base ends the turn at — but it also means
// a cap-raising facility (Hab Complex / Habitation Dome) completing in BaseProduction cannot
// unblock growth until the following turn. The two rules want opposite orders and one stage
// cannot satisfy both. Blocked growth banks its nutrients without spending the threshold
// (PopulationManager::ApplyGrowth), so the cost is a one-turn delay rather than a lost pop.
// Needs a rules decision before the cap becomes effect-driven; SMAC's own behaviour here is not
// recorded in any source found.
StageResult_t BaseGrowth::ExecuteImpl(GameState& /*rGameState*/, Faction& rFaction)
{
    std::cout << "Executing BaseGrowth stage for faction\n";

    rFaction.ApplyBaseGrowth();

    // PopulationManager::AddPop deliberately does not reconcile the drone/talent split — the
    // caller names the type it wants, and reconciling inside the add would overwrite it. So a
    // base that grew is left describing the size it no longer has until something asks. Do it
    // here, because BaseProduction and every stage after it read pop-generated effects and must
    // not see a split for the previous size. (RemovePop already reconciles itself, so a base
    // that starved needs nothing.)
    for (BaseManager& rBase : rFaction.Bases())
    {
        rBase.GetPopulation().EnsureCompositionCurrent();
    }
    return StageResult_t::Continue;
}

} // namespace ac

#include "game/stages/Population.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/GameState.h"
#include "game/PlayerInteraction.h"
#include "game/PlayerInteractionQueue.h"
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
    // World/council extras are already in Faction::GetActiveEffects via BindWorldEffects.
    // A base that starves to nothing is razed by the pop-loss handler as it happens, so it
    // drops out of Bases() before this loop reaches it.
    rFaction.ApplyBaseGrowth();

    for (BaseManager& rBase : rFaction.Bases())
    {
        ProcessBase_(rGameState, rFaction, rBase);
    }
    return StageResult_t::Continue;
}

void Population::ProcessBase_(GameState& rGameState, Faction& rFaction, BaseManager& rBase)
{
    PopulationManager& rPopulation = rBase.GetPopulation();
    rPopulation.AdvanceAssimilation();
    rPopulation.EnsureCompositionCurrent();
    rPopulation.ForecastMood();
    EnqueuePendingMoodNotices_(rGameState, rFaction, rBase);
}

void Population::EnqueuePendingMoodNotices_(GameState& rGameState, Faction& rFaction,
                                            BaseManager& rBase)
{
    if (!rFaction.IsPlayerControlled())
    {
        return;
    }

    const PopulationManager& rPopulation = rBase.GetPopulation();
    const TileCoord_t at{rBase.GetTile().GetX(), rBase.GetTile().GetY()};
    if (rPopulation.IsPendingRiot())
    {
        EnqueueForPlayer(
            rGameState,
            NoticeInteraction_t{
                PauseOnEventId_t::DroneRiots,
                "Drone Riots",
                "Drones threaten to riot at " + rBase.GetName()
                    + ". Adjust specialists or psych before the turn ends.",
                at,
            });
    }
    // TODO: unlike a riot, a pending golden age is not something the player can or would want
    // to avert, so warning about it before Mood commits may be noise rather than a decision.
    // Whether a golden age should announce on the verge or only on arrival is an unrecorded
    // rules question; the forecast/commit split itself is still needed to keep both moods on
    // one lifecycle.
    if (rPopulation.IsPendingGoldenAge())
    {
        EnqueueForPlayer(
            rGameState,
            NoticeInteraction_t{
                PauseOnEventId_t::GoldenAgeStarts,
                "Golden Age",
                rBase.GetName() + " is on the verge of a Golden Age.",
                at,
            });
    }
}

} // namespace ac

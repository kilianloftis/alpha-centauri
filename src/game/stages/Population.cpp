#include "game/stages/Population.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/GameState.h"
#include "game/PlayerInteraction.h"
#include "game/PlayerInteractionQueue.h"
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
        if (rBase.GetPopulation().GetSize() == 0)
        {
            // Collected, not razed in place: RazeBase destroys the BaseManager, which would
            // invalidate this iteration.
            depopulated.push_back(rBase.GetBaseId());
            continue;
        }
        ProcessLivingBase_(rGameState, rFaction, rBase);
    }

    RazeDepopulatedBases_(rGameState, rFaction, depopulated);
    return StageResult_t::Continue;
}

void Population::ProcessLivingBase_(GameState& rGameState, Faction& rFaction, BaseManager& rBase)
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

void Population::RazeDepopulatedBases_(GameState& rGameState, Faction& rFaction,
                                       const std::vector<BaseId_t>& rDepopulated)
{
    // A base that starves to nothing is razed, through the same pathway conquest uses.
    for (const BaseId_t baseId : rDepopulated)
    {
        if (BaseManager* pBase = rFaction.FindBase(baseId))
        {
            rGameState.RazeBase(*pBase);
        }
    }
}

} // namespace ac

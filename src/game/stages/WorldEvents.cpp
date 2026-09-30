#include "game/stages/WorldEvents.h"
#include "game/GameState.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/map/TerraformSpread.h"
#include "game/TurnStageRegistrar.h"

#include <vector>

namespace ac
{

namespace
{

TurnStageRegistrar<WorldEvents> g_registrar("WorldEvents");

void ApplyToEveryFaction_(GameState& rGameState,
                          const std::vector<TriggeredEffectConfig_t>& rEffects)
{
    if (rEffects.empty())
    {
        return;
    }
    std::vector<Faction*> factions;
    for (Faction& rFaction : rGameState.Factions())
    {
        factions.push_back(&rFaction);
    }
    TriggeredEffectContext_t context(rGameState, std::move(factions));
    ApplyTriggeredEffects(rEffects, context);
}

void AdvanceWorldEvents_(GameState& rGameState)
{
    WorldEventTracker* pEvents = rGameState.GetWorldEvents();
    if (!pEvents)
    {
        return;
    }
    const WorldEventTracker::Transitions_t transitions =
        pEvents->Advance(rGameState.GetYearsSinceFirstPlayableYear());
    for (const WorldEventConfig_t* pEvent : transitions.ended)
    {
        ApplyToEveryFaction_(rGameState, pEvent->onEndEffects);
    }
    for (const WorldEventConfig_t* pEvent : transitions.started)
    {
        ApplyToEveryFaction_(rGameState, pEvent->onStartEffects);
    }
}

} // namespace

WorldEvents::WorldEvents(HookContext hookContext)
    : GlobalTurnStage(std::move(hookContext))
{
}

StageResult_t WorldEvents::ExecuteImpl(GameState& rGameState)
{
    // Cyclic events are not random events, so randomEventsAfterTurn does not gate them.
    // TODO(difficulty): gate random events on the session level's
    // rules.randomEventsAfterTurn vs years/turns.
    AdvanceWorldEvents_(rGameState);
    SpreadTerraformImprovements(rGameState.GetWorldMap(), rGameState.GetTileEffects(),
                                rGameState.GetYearsSinceFirstPlayableYear(), rGameState.GetRng());
    return StageResult_t::Continue;
}

} // namespace ac

#include "game/stages/WorldEvents.h"
#include "game/GameState.h"
#include "game/map/TerraformSpread.h"
#include "game/TurnStageRegistrar.h"

namespace ac
{

namespace { TurnStageRegistrar<WorldEvents> g_registrar("WorldEvents"); }

WorldEvents::WorldEvents(HookContext hookContext)
    : GlobalTurnStage(std::move(hookContext))
{
}

StageResult_t WorldEvents::ExecuteImpl(GameState& rGameState)
{
    SpreadTerraformImprovements(rGameState.GetWorldMap(), rGameState.GetTileEffects(),
                                rGameState.GetYearsSinceFirstPlayableYear(), rGameState.GetRng());
    return StageResult_t::Continue;
}

} // namespace ac

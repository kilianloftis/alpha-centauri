#include "game/stages/WorldEvents.h"
#include "game/GameState.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/TerraformSpread.h"
#include "game/map/WorldMap.h"
#include "game/TurnStageRegistrar.h"

#include <algorithm>
#include <random>

namespace ac
{

namespace { TurnStageRegistrar<WorldEvents> g_registrar("WorldEvents"); }

WorldEvents::WorldEvents(HookContext hookContext)
    : GlobalTurnStage(std::move(hookContext))
{
}

StageResult_t WorldEvents::ExecuteImpl(GameState& rGameState)
{
    // First playable year is 2100; SMAC CurrentTurn is years since then.
    const int turnIndex = std::max(0, rGameState.GetMissionYear() - 2100);
    const WorldMap& rMap = rGameState.GetWorldMap();
    std::mt19937 rng(static_cast<std::mt19937::result_type>(
        static_cast<unsigned>(turnIndex) * 0x9E3779B9u
        ^ static_cast<unsigned>(rMap.GetWidth() * rMap.GetHeight())));
    SpreadTerraformImprovements(rGameState.GetWorldMap(), rGameState.GetTileEffects(),
                                turnIndex, rng);
    return StageResult_t::Continue;
}

} // namespace ac

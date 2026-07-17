#include "game/stages/WorldEvents.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<WorldEvents> g_registrar("WorldEvents"); }

WorldEvents::WorldEvents(HookContext hookContext)
    : GlobalTurnStage(std::move(hookContext))
{
}

StageResult_t WorldEvents::ExecuteImpl(GameState& rGameState)
{
    (void)rGameState;
    std::cout << "Executing WorldEvents stage\n";
    return StageResult_t::Continue;
}

} // namespace ac

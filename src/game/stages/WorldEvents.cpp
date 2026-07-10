#include "game/stages/WorldEvents.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<WorldEvents> g_registrar("WorldEvents"); }

WorldEvents::WorldEvents(std::shared_ptr<HookContext> pHookContext)
    : GlobalTurnStage(pHookContext)
{
}

void WorldEvents::ExecuteImpl(GameState& rGameState)
{
    (void)rGameState;
    std::cout << "Executing WorldEvents stage\n";
}

} // namespace ac

#include "game/stages/TurnStart.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<TurnStart> g_registrar("TurnStart"); }

TurnStart::TurnStart(std::shared_ptr<HookContext> pHookContext)
    : GlobalTurnStage(pHookContext)
{
}

void TurnStart::ExecuteImpl(GameState& rGameState)
{
    (void)rGameState;
    std::cout << "Executing TurnStart stage\n";
}

} // namespace ac

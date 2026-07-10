#include "game/stages/TurnEnd.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<TurnEnd> g_registrar("TurnEnd"); }

TurnEnd::TurnEnd(std::shared_ptr<HookContext> pHookContext)
    : GlobalTurnStage(pHookContext)
{
}

void TurnEnd::ExecuteImpl(GameState& rGameState)
{
    (void)rGameState;
    std::cout << "Executing TurnEnd stage\n";
}

} // namespace ac

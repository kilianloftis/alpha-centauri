#include "game/stages/NewYearBegins.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<NewYearBegins> g_registrar("NewYearBegins"); }

NewYearBegins::NewYearBegins(std::shared_ptr<HookContext> pHookContext)
    : GlobalTurnStage(pHookContext)
{
}

void NewYearBegins::ExecuteImpl(GameState& rGameState)
{
    (void)rGameState;
    std::cout << "Executing NewYearBegins stage\n";
}

} // namespace ac

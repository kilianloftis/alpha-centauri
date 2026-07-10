#include "game/stages/Save.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<Save> g_registrar("Save"); }

Save::Save(std::shared_ptr<HookContext> pHookContext)
    : GlobalTurnStage(pHookContext)
{
}

void Save::ExecuteImpl(GameState& rGameState)
{
    (void)rGameState;
    std::cout << "Executing Save stage\n";
}

} // namespace ac

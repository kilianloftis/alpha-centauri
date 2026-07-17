#include "game/stages/Save.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<Save> g_registrar("Save"); }

Save::Save(HookContext hookContext)
    : GlobalTurnStage(std::move(hookContext))
{
}

StageResult_t Save::ExecuteImpl(GameState& rGameState)
{
    (void)rGameState;
    std::cout << "Executing Save stage\n";
    return StageResult_t::Continue;
}

} // namespace ac

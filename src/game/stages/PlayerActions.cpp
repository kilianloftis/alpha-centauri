#include "game/stages/PlayerActions.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<PlayerActions> g_registrar("PlayerActions"); }

PlayerActions::PlayerActions(std::shared_ptr<HookContext> pHookContext)
    : PerFactionTurnStage(pHookContext)
{
}

void PlayerActions::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    (void)rGameState;
    (void)rFaction;
    std::cout << "Executing PlayerActions stage\n";
}

} // namespace ac

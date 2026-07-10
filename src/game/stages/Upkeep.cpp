#include "game/stages/Upkeep.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<Upkeep> g_registrar("Upkeep"); }

Upkeep::Upkeep(std::shared_ptr<HookContext> pHookContext)
    : PerFactionTurnStage(pHookContext)
{
}

void Upkeep::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    (void)rGameState;
    (void)rFaction;
    std::cout << "Executing Upkeep stage\n";
}

} // namespace ac

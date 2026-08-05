#include "game/stages/Upkeep.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<Upkeep> g_registrar("Upkeep"); }

Upkeep::Upkeep(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t Upkeep::ExecuteImpl(GameState& /*rGameState*/, Faction& /*rFaction*/)
{
    std::cout << "Executing Upkeep stage\n";
    return StageResult_t::Continue;
}

} // namespace ac

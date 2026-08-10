#include "game/stages/Upkeep.h"
#include "game/Faction.h"
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

StageResult_t Upkeep::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    std::cout << "Executing Upkeep stage\n";
    // Retire ASAT / interceptor deploy records that have come off cooldown. Done here rather
    // than inside CountReadyBuildings so that query stays pure (see Faction::CountReadyBuildings).
    rFaction.PruneExpiredDeploys(rGameState.GetMissionYear());
    // Mineral support runs after ResourceCollection banks minerals and before BaseProduction
    // spends the remainder on the build queue.
    rFaction.ApplyMineralSupport();
    return StageResult_t::Continue;
}

} // namespace ac

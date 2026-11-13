#include "game/stages/Upkeep.h"
#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/combat/TileInterceptDeployLedger.h"
#include "game/stages/TurnStageRegistrar.h"
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
    // Retire ASAT / interceptor deploy records that have come off cooldown; here, so
    const int year = rGameState.GetMissionYear();
    rFaction.GetBuildingDeploys().PruneExpired(year);
    rGameState.GetTileInterceptDeploys().PruneExpired(year);
    // Facility energy upkeep runs after IncomeCollection has banked this turn's econ.
    rFaction.ApplyBuildingUpkeep();
    return StageResult_t::Continue;
}

} // namespace ac

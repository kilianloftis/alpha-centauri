#include "game/stages/UnitSupport.h"
#include "game/Faction.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<UnitSupport> g_registrar("UnitSupport"); }

UnitSupport::UnitSupport(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t UnitSupport::ExecuteImpl(GameState& /*rGameState*/, Faction& rFaction)
{
    std::cout << "Executing UnitSupport stage for faction\n";

    rFaction.ApplyMineralSupport();
    return StageResult_t::Continue;
}

} // namespace ac

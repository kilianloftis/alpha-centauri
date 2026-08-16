#include "game/stages/MineralConversion.h"
#include "game/Faction.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<MineralConversion> g_registrar("MineralConversion"); }

MineralConversion::MineralConversion(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t MineralConversion::ExecuteImpl(GameState& /*rGameState*/, Faction& rFaction)
{
    std::cout << "Executing MineralConversion stage for faction\n";

    rFaction.ConvertMinerals();
    return StageResult_t::Continue;
}

} // namespace ac

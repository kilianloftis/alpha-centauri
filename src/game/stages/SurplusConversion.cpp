#include "game/stages/SurplusConversion.h"
#include "game/Faction.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<SurplusConversion> g_registrar("SurplusConversion"); }

SurplusConversion::SurplusConversion(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t SurplusConversion::ExecuteImpl(GameState& /*rGameState*/, Faction& rFaction)
{
    std::cout << "Executing SurplusConversion stage for faction\n";

    rFaction.ConvertSurplusMinerals();
    return StageResult_t::Continue;
}

} // namespace ac

#include "game/stages/ResourceCollection.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<ResourceCollection> g_registrar("ResourceCollection"); }

ResourceCollection::ResourceCollection(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t ResourceCollection::ExecuteImpl(GameState& /*rGameState*/, Faction& rFaction)
{
    std::cout << "Executing ResourceCollection stage for faction\n";

    // World/council extras are already in Faction::GetActiveEffects via BindWorldEffects.
    rFaction.ProduceBaseResources();
    // Support claims this turn's minerals first; leftover is converted if a stockpile is
    // queued (or wasted) so income / research / growth see those credits this turn.
    rFaction.ApplyMineralSupport();
    rFaction.ConvertSurplusMinerals();
    return StageResult_t::Continue;
}

} // namespace ac

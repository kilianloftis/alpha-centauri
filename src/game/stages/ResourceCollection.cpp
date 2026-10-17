#include "game/stages/ResourceCollection.h"
#include "game/GameState.h"
#include "game/faction/Faction.h"
#include "game/base/BaseManager.h"
#include "game/stages/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<ResourceCollection> g_registrar("ResourceCollection"); }

ResourceCollection::ResourceCollection(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t ResourceCollection::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    std::cout << "Executing ResourceCollection stage for faction\n";

    // World/council extras come with Faction::GetActiveEffects, from the current game state.
    rFaction.ProduceBaseResources();
    return StageResult_t::Continue;
}

} // namespace ac

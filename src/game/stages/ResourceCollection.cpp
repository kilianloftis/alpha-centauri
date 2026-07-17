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

StageResult_t ResourceCollection::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    std::cout << "Executing ResourceCollection stage for faction\n";

    // Other factions' WorldGlobal effects apply here too (the faction's own pool
    // already includes its own).
    const std::vector<ActiveEffect_t> worldEffects = rGameState.CollectWorldEffects(rFaction);
    rFaction.ProduceBaseResources(worldEffects);
    return StageResult_t::Continue;
}

} // namespace ac

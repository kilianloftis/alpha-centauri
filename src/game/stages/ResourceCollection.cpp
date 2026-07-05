#include "game/stages/ResourceCollection.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include <iostream>

namespace ac
{

ResourceCollection::ResourceCollection(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void ResourceCollection::Execute_(GameState* pGameState, Faction* pFaction)
{
    if (!pFaction)
    {
        std::cout << "Executing ResourceCollection stage (no faction)\n";
        return;
    }

    std::cout << "Executing ResourceCollection stage for faction\n";

    // Other factions' WorldGlobal effects apply here too (the faction's own pool
    // already includes its own).
    const std::vector<ActiveEffect_t> worldEffects =
        pGameState ? pGameState->CollectWorldEffects(*pFaction) : std::vector<ActiveEffect_t>{};
    pFaction->ProduceBaseResources(worldEffects);
}

} // namespace ac

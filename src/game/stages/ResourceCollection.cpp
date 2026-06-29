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
    (void)pGameState;

    if (!pFaction)
    {
        std::cout << "Executing ResourceCollection stage (no faction)\n";
        return;
    }

    std::cout << "Executing ResourceCollection stage for faction\n";

    pFaction->ProduceBaseResources();
}

} // namespace ac

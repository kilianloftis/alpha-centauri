#include "game/stages/ResourceCollection.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/BaseEconomyManager.h"
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

    // Get the faction's economy manager for energy allocation
    BaseEconomyManager* pEconomy = pFaction->GetEconomy();

    // Collect resources from all bases
    for (size_t i = 0; i < pFaction->GetBaseCount(); ++i)
    {
        BaseManager* pBase = pFaction->GetBase(i);
        if (pBase)
        {
            pBase->CollectResources(pEconomy);
        }
    }
}

} // namespace ac

#include "game/stages/BaseProduction.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include <iostream>

namespace ac
{

BaseProduction::BaseProduction(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void BaseProduction::Execute_(GameState* pGameState, Faction* pFaction)
{
    (void)pGameState;

    if (!pFaction)
    {
        std::cout << "Executing BaseProduction stage (no faction)\n";
        return;
    }

    std::cout << "Executing BaseProduction stage for faction\n";

    for (size_t i = 0; i < pFaction->GetBaseCount(); ++i)
    {
        BaseManager* pBase = pFaction->GetBase(i);
        if (!pBase)
        {
            continue;
        }

        ResourceManager* pResources = pBase->GetResourceManager();
        if (!pResources)
        {
            continue;
        }

        // Resources already collected in ResourceCollection stage
        // Note: Energy is NOT stockpiled - it flows directly to faction-level allocation
        std::cout << "  Base '" << pBase->GetName() << "' resource production:"
                  << " nutrients=" << pResources->GetNutrientProduction()
                  << " minerals=" << pResources->GetMineralProduction()
                  << " energy=" << pResources->GetEnergyProduction() << "\n";

        std::cout << "  Stockpiles: nutrients=" << pResources->GetNutrientStockpile()
                  << ", minerals=" << pResources->GetMineralStockpile()
                  << " (energy not stockpiled)\n";
    }
}

} // namespace ac

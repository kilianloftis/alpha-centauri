#include "game/stages/BaseProduction.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
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

    for (const auto& pBase : pFaction->GetBases())
    {
        const IConstructable* pCurrentProduction = pBase->GetCurrentProduction();
        if (!pCurrentProduction)
        {
            continue;
        }

        const int cost = pBase->GetProductionMineralCost();
        if (pBase->GetMineralStockpile() >= cost)
        {
            pBase->ConsumeMinerals(cost);
            const std::string completed = pBase->CompleteProduction();
            std::cout << "  Base '" << pBase->GetName() << "' completed production: " << completed << "\n";
        }
        else
        {
            std::cout << "  Base '" << pBase->GetName() << "' producing '" << pCurrentProduction->GetName()
                      << "' (" << pBase->GetMineralStockpile() << "/" << cost << " minerals)\n";
        }
    }
}

} // namespace ac

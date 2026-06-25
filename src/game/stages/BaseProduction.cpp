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
        if (!pBase->GetCurrentProduction())
        {
            continue;
        }

        const std::string completed = pBase->ApplyProduction();
        if (!completed.empty())
        {
            std::cout << "  Base '" << pBase->GetName() << "' completed production: " << completed << "\n";
        }
        else
        {
            std::cout << "  Base '" << pBase->GetName() << "' producing '"
                      << pBase->GetCurrentProduction()->GetName()
                      << "' (" << pBase->GetMineralStockpile() << "/" << pBase->GetProductionMineralCost() << " minerals)\n";
        }
    }
}

} // namespace ac

#include "game/stages/BaseProduction.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
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

    for (BaseManager& rBase : pFaction->Bases())
    {
        const ProductionManager& rProduction = rBase.GetProduction();
        if (!rProduction.GetCurrentProduction())
        {
            continue;
        }

        const std::string completed = rBase.ApplyProduction();
        if (!completed.empty())
        {
            std::cout << "  Base '" << rBase.GetName() << "' completed production: " << completed << "\n";
        }
        else
        {
            std::cout << "  Base '" << rBase.GetName() << "' producing '"
                      << rProduction.GetCurrentProduction()->GetName()
                      << "' (" << rProduction.GetMineralStockpile() << "/"
                      << rBase.GetMineralCost() << " minerals)\n";
        }
    }
}

} // namespace ac

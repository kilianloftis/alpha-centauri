#include "game/stages/BaseProduction.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<BaseProduction> g_registrar("BaseProduction"); }

BaseProduction::BaseProduction(std::shared_ptr<HookContext> pHookContext)
    : PerFactionTurnStage(pHookContext)
{
}

void BaseProduction::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    (void)rGameState;

    std::cout << "Executing BaseProduction stage for faction\n";

    for (BaseManager& rBase : rFaction.Bases())
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

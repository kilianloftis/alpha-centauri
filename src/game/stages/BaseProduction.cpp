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

    for (size_t i = 0; i < pFaction->GetBaseCount(); ++i)
    {
        BaseManager* pBase = pFaction->GetBase(i);
        if (!pBase)
        {
            continue;
        }

        // Resources already collected in ResourceCollection stage
        // Note: Energy is NOT stockpiled - it flows directly to faction-level allocation
        std::cout << "  Base '" << pBase->GetName() << "' resource production:"
                  << " nutrients=" << pBase->GetNutrientProduction()
                  << " minerals=" << pBase->GetMineralProduction()
                  << " energy=" << pBase->GetEnergyProduction() << "\n";

        std::cout << "  Stockpiles: nutrients=" << pBase->GetNutrientStockpile()
                  << ", minerals=" << pBase->GetMineralStockpile()
                  << " (energy not stockpiled)\n";

        pBase->ProcessProduction();

        const std::string& currentProduction = pBase->GetProduction();
        if (!currentProduction.empty())
        {
            std::cout << "  Producing: " << currentProduction
                      << " (" << pBase->GetProductionAccumulatedMinerals()
                      << "/" << pBase->GetProductionMineralCost() << " minerals)\n";
        }
    }
}

} // namespace ac

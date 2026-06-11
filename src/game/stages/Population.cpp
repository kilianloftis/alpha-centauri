#include "game/stages/Population.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/population/calculators/GrowthCalculator.h"
#include "game/faction/base/population/calculators/PopCompositionCalculator.h"
#include <iostream>

namespace ac
{

Population::Population(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void Population::Execute_(GameState* pGameState, Faction* pFaction)
{
    std::cout << "Executing Population stage\n";

    (void)pGameState;

    if (!pFaction)
    {
        return;
    }

    for (auto& pBase : pFaction->GetBases())
    {
        if (pBase && pBase->GetPopulation())
        {
            PopulationManager* pPop = pBase->GetPopulation();

            // Population growth uses nutrients directly from production (for the growth bank).
            // The nutrient stockpile (for spending on production rushing, etc.) is separate
            // and is accumulated during the BaseProduction stage.
            std::cout << "  Accumulating growth for base '" << pBase->GetName() << "' (bank: " << pBase->GetNutrientStockpile() << " -> ";

            GrowthInputs_t inputs;
            inputs.baseSize = pBase->GetBaseSize();
            inputs.growthRateModifier = pBase->GetGrowthRate();
            inputs.nutrientBank = pBase->GetNutrientStockpile();

            int newNutrientBank = 0;
            const GrowthResult result = GrowthCalculator::CalculateGrowh(inputs, newNutrientBank);

            pBase->ApplyGrowthResult(result, newNutrientBank);

            std::cout << newNutrientBank << ", size: " << pPop->GetSize() << ")\n";

            pPop->RecalculateComposition();
        }
    }
}

} // namespace ac

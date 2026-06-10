#include "game/stages/Population.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/population/calculators/PopCompositionCalculator.h"
#include <iostream>

namespace ac
{

Population::Population(std::shared_ptr<HookContext> hookContext, PopCompositionCalculator* pCalculator)
    : TurnStageBase(hookContext)
    , m_pCalculator(pCalculator)
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

    for (size_t i = 0; i < pFaction->GetBaseCount(); ++i)
    {
        BaseManager* pBase = pFaction->GetBase(i);
        if (pBase && pBase->GetPopulation())
        {
            PopulationManager* pPop = pBase->GetPopulation();
            pPop->SetCompositionCalculator(m_pCalculator);

            // Population growth uses nutrients directly from production (for the growth bank).
            // The nutrient stockpile (for spending on production rushing, etc.) is separate
            // and is accumulated during the BaseProduction stage.
            std::cout << "  Accumulating growth for base '" << pBase->GetName() << "' (bank: " << pPop->GetNutrientBank() << " -> ";
            ResourceManager* pResources = pBase->GetResourceManager();
            int nutrientProduction = pResources ? pResources->GetNutrientProduction() : 0;
            pPop->AccumulateGrowth(nutrientProduction);
            std::cout << pPop->GetNutrientBank() << ", size: " << pPop->GetSize() << ")\n";

            pPop->RecalculateComposition();
            pPop->CheckRiotEndOfTurn();
        }
    }
}

} // namespace ac

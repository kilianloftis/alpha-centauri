#include "game/stages/Population.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/population/calculators/GrowthCalculator.h"
#include <iostream>

namespace ac
{

Population::Population(std::shared_ptr<HookContext> hookContext, GrowthCalculator* pGrowthCalculator)
    : TurnStageBase(hookContext)
    , m_pGrowthCalculator(pGrowthCalculator)
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
        if (!pBase)
        {
            continue;
        }

        const int stockpile = pBase->GetNutrientStockpile();
        std::cout << "  Accumulating growth for base '" << pBase->GetName() << "' (bank: " << stockpile << " -> ";

        if (m_pGrowthCalculator)
        {
            const int required = m_pGrowthCalculator->ComputeNutrientsRequired(
                pBase->GetBaseSize(), pBase->GetGrowthRate());

            if (stockpile >= required)
            {
                pBase->AddPop();
                pBase->SetNutrientStockpile(stockpile - required);
            }
            else if (stockpile < 0)
            {
                pBase->RemovePop();
                pBase->SetNutrientStockpile(0);
            }
        }

        std::cout << pBase->GetNutrientStockpile() << ", size: " << pBase->GetBaseSize() << ")\n";

        pBase->RecalculatePopComposition();
    }
}

} // namespace ac

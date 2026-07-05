#include "game/stages/Population.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
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

    if (!pFaction)
    {
        return;
    }

    // Other factions' WorldGlobal effects apply to growth too.
    const std::vector<ActiveEffect_t> worldEffects =
        pGameState ? pGameState->CollectWorldEffects(*pFaction) : std::vector<ActiveEffect_t>{};
    pFaction->ApplyBaseGrowth(worldEffects);

    for (auto& pBase : pFaction->GetBases())
    {
        if (!pBase)
        {
            continue;
        }
        std::cout << "  Growth applied for base '" << pBase->GetName()
                  << "' (bank: " << pBase->GetNutrientStockpile()
                  << ", size: " << pBase->GetBaseSize() << ")\n";
        pBase->RecalculatePopComposition();
    }
}

} // namespace ac

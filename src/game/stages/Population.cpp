#include "game/stages/Population.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
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

    for (BaseManager& rBase : pFaction->Bases())
    {
        std::cout << "  Growth applied for base '" << rBase.GetName()
                  << "' (bank: " << rBase.GetPopulation().GetNutrientStockpile()
                  << ", size: " << rBase.GetPopulation().GetSize() << ")\n";
        rBase.GetPopulation().RecalculateComposition();
    }
}

} // namespace ac

#include "game/stages/Population.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<Population> g_registrar("Population"); }

Population::Population(std::shared_ptr<HookContext> pHookContext)
    : PerFactionTurnStage(pHookContext)
{
}

void Population::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    std::cout << "Executing Population stage\n";

    // Other factions' WorldGlobal effects apply to growth too.
    const std::vector<ActiveEffect_t> worldEffects = rGameState.CollectWorldEffects(rFaction);
    rFaction.ApplyBaseGrowth(worldEffects);

    for (BaseManager& rBase : rFaction.Bases())
    {
        std::cout << "  Growth applied for base '" << rBase.GetName()
                  << "' (bank: " << rBase.GetPopulation().GetNutrientStockpile()
                  << ", size: " << rBase.GetPopulation().GetSize() << ")\n";
        rBase.GetPopulation().RecalculateComposition();
    }
}

} // namespace ac

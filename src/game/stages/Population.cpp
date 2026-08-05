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

Population::Population(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t Population::ExecuteImpl(GameState& /*rGameState*/, Faction& rFaction)
{
    std::cout << "Executing Population stage\n";

    // World/council extras are already in Faction::GetActiveEffects via BindWorldEffects.
    rFaction.ApplyBaseGrowth();

    for (BaseManager& rBase : rFaction.Bases())
    {
        std::cout << "  Growth applied for base '" << rBase.GetName()
                  << "' (bank: " << rBase.GetPopulation().GetNutrientStockpile()
                  << ", size: " << rBase.GetPopulation().GetSize() << ")\n";
        rBase.GetPopulation().RecalculateComposition();
    }
    return StageResult_t::Continue;
}

} // namespace ac

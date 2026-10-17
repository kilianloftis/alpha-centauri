#include "game/stages/IncomeCollection.h"
#include "game/GameState.h"
#include "game/faction/Faction.h"
#include "game/economy/EconomyManager.h"
#include "game/stages/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<IncomeCollection> g_registrar("IncomeCollection"); }

IncomeCollection::IncomeCollection(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t IncomeCollection::ExecuteImpl(GameState& /*rGameState*/, Faction& rFaction)
{
    std::cout << "Executing IncomeCollection stage for faction\n";

    const int totalIncome = rFaction.CollectIncome();

    std::cout << "  Faction income: " << totalIncome
              << ", total energy: " << rFaction.GetEconomy().GetEnergy() << "\n";
    return StageResult_t::Continue;
}

} // namespace ac

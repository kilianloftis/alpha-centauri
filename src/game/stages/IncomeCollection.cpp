#include "game/stages/IncomeCollection.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/EconomyManager.h"
#include "game/TurnStageRegistrar.h"
#include <iostream>

namespace ac
{

namespace { TurnStageRegistrar<IncomeCollection> g_registrar("IncomeCollection"); }

IncomeCollection::IncomeCollection(std::shared_ptr<HookContext> pHookContext)
    : PerFactionTurnStage(pHookContext)
{
}

void IncomeCollection::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    (void)rGameState;

    std::cout << "Executing IncomeCollection stage for faction\n";

    const int totalIncome = rFaction.CollectIncome();

    std::cout << "  Faction income: " << totalIncome
              << ", total energy: " << rFaction.GetEconomy().GetEnergy() << "\n";
}

} // namespace ac

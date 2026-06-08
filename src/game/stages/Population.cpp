#include "game/stages/Population.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/Base.h"
#include "game/faction/population/BasePopulation.h"
#include <iostream>

namespace ac
{

Population::Population(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void Population::Execute_(GameState* pGameState)
{
    std::cout << "Executing Population stage\n";

    if (!pGameState)
    {
        return;
    }

    // Grow population for all bases in all factions
    for (auto& pFaction : pGameState->GetFactions())
    {
        for (size_t i = 0; i < pFaction->GetBaseCount(); ++i)
        {
            Base* pBase = pFaction->GetBase(i);
            if (pBase && pBase->GetPopulation())
            {
                std::cout << "  Growing base '" << pBase->GetName() << "' (" << pBase->GetPopulation()->GetSize() << " -> ";
                pBase->AddPop();
                std::cout << pBase->GetPopulation()->GetSize() << ")\n";
            }
        }
    }
}

} // namespace ac

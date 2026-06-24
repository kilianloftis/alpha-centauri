#include "game/stages/ResearchAccumulation.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include <iostream>

namespace ac
{

ResearchAccumulation::ResearchAccumulation(std::shared_ptr<HookContext> hookContext)
    : TurnStageBase(hookContext)
{
}

void ResearchAccumulation::Execute_(GameState* pGameState, Faction* pFaction)
{
    (void)pGameState;

    if (!pFaction)
    {
        std::cout << "Executing ResearchAccumulation stage (no faction)\n";
        return;
    }

    std::cout << "Executing ResearchAccumulation stage for faction\n";

    int totalLabs = 0;

    for (size_t i = 0; i < pFaction->GetBaseCount(); ++i)
    {
        BaseManager* pBase = pFaction->GetBase(i);
        if (!pBase)
        {
            continue;
        }

        int baseLabs = pBase->ConsumeLabs();
        totalLabs += baseLabs;

        std::cout << "  Base '" << pBase->GetName() << "' labs: " << baseLabs << "\n";
    }

    // TODO: Apply any modifiers (e.g., from facilities, techs, etc.)
    pFaction->AddResearchPoints(totalLabs);

    std::cout << "  Faction total research points: " << pFaction->GetResearchPoints() << "\n";
}

} // namespace ac

#include "game/TurnProcessor.h"
#include "game/TurnStages.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include <iostream>

namespace ac
{

TurnProcessor::TurnProcessor(TurnStageRegistry_t registry, TurnStageRepeatFlags_t repeatFlags, std::vector<std::string> stageOrder)
    : m_missionYear(0)
    , m_registry(std::move(registry))
    , m_repeatFlags(std::move(repeatFlags))
    , m_stageOrder(std::move(stageOrder))
{}

void TurnProcessor::ProcessTurn(int missionYear, int numFactions, GameState& rGameState)
{
    m_missionYear = missionYear;
    std::cout << "\n--- Mission Year " << m_missionYear << " ---\n";

    for (const auto& stageId : m_stageOrder)
    {
        auto it = m_registry.find(stageId);
        if (it != m_registry.end())
        {
            auto& stage = it->second;
            auto flagIt = m_repeatFlags.find(stageId);
            bool bRepeatForEachFaction = (flagIt != m_repeatFlags.end()) && flagIt->second;

            stage->OnEnter();
            if (bRepeatForEachFaction)
            {
                for (Faction& rFaction : rGameState.Factions())
                {
                    stage->Execute(&rGameState, &rFaction);
                }
            }
            else
            {
                stage->Execute(&rGameState);
            }
            stage->OnExit();
        }
    }
}
} // namespace ac

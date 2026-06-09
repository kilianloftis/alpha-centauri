#include "game/TurnProcessor.h"
#include "game/TurnStages.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include <magic_enum.hpp>
#include <iostream>

namespace ac
{

TurnProcessor::TurnProcessor(TurnStageRegistry_t registry, TurnStageRepeatFlags_t repeatFlags)
    : m_missionYear(0)
    , m_registry(std::move(registry))
    , m_repeatFlags(std::move(repeatFlags))
{}

void TurnProcessor::ProcessTurn(int missionYear, int numFactions, GameState& rGameState)
{
    m_missionYear = missionYear;
    std::cout << "\n--- Mission Year " << m_missionYear << " ---\n";

    auto enumValues = magic_enum::enum_values<TurnStage>();
    for (const auto& stageEnum : enumValues)
    {
        if (stageEnum == TurnStage::Count)
        {
            continue;
        }

        auto it = m_registry.find(stageEnum);
        if (it != m_registry.end())
        {
            auto& stage = it->second;
            auto flagIt = m_repeatFlags.find(stageEnum);
            bool bRepeatForEachFaction = (flagIt != m_repeatFlags.end()) && flagIt->second;

            stage->OnEnter();
            if (bRepeatForEachFaction)
            {
                for (auto& pFaction : rGameState.GetFactions())
                {
                    stage->Execute(&rGameState, pFaction.get());
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

#include "game/TurnProcessor.h"
#include "game/TurnStages.h"
#include "game/GameState.h"
#include <magic_enum.hpp>
#include <iostream>

namespace ac
{

TurnProcessor::TurnProcessor(TurnStageRegistry_t registry)
    : m_missionYear(0)
    , m_registry(std::move(registry))
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
            stage->OnEnter();
            stage->Execute(&rGameState);
            stage->OnExit();
        }
    }
}
} // namespace ac

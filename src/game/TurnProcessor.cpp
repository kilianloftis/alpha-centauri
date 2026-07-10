#include "game/TurnProcessor.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include <iostream>
#include <stdexcept>

namespace ac
{

TurnProcessor::TurnProcessor(GlobalTurnStageRegistry_t globalRegistry,
                              PerFactionTurnStageRegistry_t perFactionRegistry,
                              std::vector<std::string> stageOrder)
    : m_globalRegistry(std::move(globalRegistry))
    , m_perFactionRegistry(std::move(perFactionRegistry))
    , m_stageOrder(std::move(stageOrder))
{}

void TurnProcessor::ProcessTurn(GameState& rGameState)
{
    std::cout << "\n--- Mission Year " << rGameState.GetMissionYear() << " ---\n";

    for (const auto& stageId : m_stageOrder)
    {
        auto globalIt = m_globalRegistry.find(stageId);
        if (globalIt != m_globalRegistry.end())
        {
            auto& stage = globalIt->second;
            stage->OnEnter();
            stage->Execute(rGameState);
            stage->OnExit();
            continue;
        }

        auto perFactionIt = m_perFactionRegistry.find(stageId);
        if (perFactionIt != m_perFactionRegistry.end())
        {
            auto& stage = perFactionIt->second;
            stage->OnEnter();
            for (Faction& rFaction : rGameState.Factions())
            {
                stage->Execute(rGameState, rFaction);
            }
            stage->OnExit();
            continue;
        }

        throw std::runtime_error("Turn stage '" + stageId + "' is in the stage order but not registered");
    }
}
} // namespace ac

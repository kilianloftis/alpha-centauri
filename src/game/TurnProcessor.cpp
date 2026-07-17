#include "game/TurnProcessor.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include <stdexcept>
#include <utility>

namespace ac
{

TurnProcessor::TurnProcessor(GlobalTurnStageRegistry_t globalRegistry,
                             PerFactionTurnStageRegistry_t perFactionRegistry,
                             std::vector<std::string> stageOrder)
    : m_globalRegistry(std::move(globalRegistry))
    , m_perFactionRegistry(std::move(perFactionRegistry))
    , m_stageOrder(std::move(stageOrder))
{
    if (m_stageOrder.empty())
    {
        throw std::logic_error("TurnProcessor: stage order is empty");
    }
}

void TurnProcessor::CompleteStage_(TurnStageBase& rStage)
{
    rStage.OnExit();
    m_bStageEntered = false;
    m_resumeFactionId.reset();
    ++m_stageIndex;
}

void TurnProcessor::EnsureEntered_(TurnStageBase& rStage)
{
    if (!m_bStageEntered)
    {
        rStage.OnEnter();
        m_bStageEntered = true;
    }
}

StageResult_t TurnProcessor::ExecuteGlobalStage_(GlobalTurnStage& rStage, GameState& rGameState)
{
    EnsureEntered_(rStage);

    const StageResult_t result = rStage.Execute(rGameState);
    if (result == StageResult_t::Yield)
    {
        return StageResult_t::Yield;
    }

    CompleteStage_(rStage);
    return StageResult_t::Continue;
}

StageResult_t TurnProcessor::ExecutePerFactionStage_(PerFactionTurnStage& rStage,
                                                     GameState& rGameState)
{
    EnsureEntered_(rStage);

    for (Faction& rFaction : rGameState.Factions())
    {
        if (m_resumeFactionId.has_value() && rFaction.GetFactionId() < *m_resumeFactionId)
        {
            continue;
        }

        const StageResult_t result = rStage.Execute(rGameState, rFaction);
        if (result == StageResult_t::Yield)
        {
            m_resumeFactionId = rFaction.GetFactionId();
            return StageResult_t::Yield;
        }
    }

    CompleteStage_(rStage);
    return StageResult_t::Continue;
}

StageResult_t TurnProcessor::ExecuteCurrentStage_(GameState& rGameState)
{
    const std::string& stageId = m_stageOrder[m_stageIndex];

    auto globalIt = m_globalRegistry.find(stageId);
    if (globalIt != m_globalRegistry.end())
    {
        return ExecuteGlobalStage_(*globalIt->second, rGameState);
    }

    auto perFactionIt = m_perFactionRegistry.find(stageId);
    if (perFactionIt != m_perFactionRegistry.end())
    {
        return ExecutePerFactionStage_(*perFactionIt->second, rGameState);
    }

    throw std::runtime_error("Turn stage '" + stageId + "' is in the stage order but not registered");
}

void TurnProcessor::Advance(GameState& rGameState)
{
    // When resuming a yielded stage, finish the current cycle and allow one complete new
    // cycle to find its next yield. A fresh cycle that never yields is misconfigured.
    bool bMayStartNextCycle = m_bStageEntered;
    while (true)
    {
        if (m_stageIndex >= m_stageOrder.size())
        {
            if (!bMayStartNextCycle)
            {
                throw std::logic_error(
                    "TurnProcessor::Advance: stage order completed without a yielding stage");
            }
            bMayStartNextCycle = false;
            m_stageIndex = 0;
        }

        if (ExecuteCurrentStage_(rGameState) == StageResult_t::Yield)
        {
            return;
        }
    }
}

} // namespace ac

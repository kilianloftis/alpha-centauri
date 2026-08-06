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

void TurnProcessor::Reset()
{
    m_stageIndex = 0;
    m_bStageEntered = false;
    m_completedFactionIds.clear();
    m_bHasResumeFaction = false;
    m_resumeFactionId = 0;
}

void TurnProcessor::CompleteStage_(TurnStageBase& rStage)
{
    // Clear entered before OnExit so a throw from OnExit cannot double-run post hooks via Abort.
    m_bStageEntered = false;
    m_completedFactionIds.clear();
    m_bHasResumeFaction = false;
    m_resumeFactionId = 0;
    rStage.OnExit();
    ++m_stageIndex;
}

void TurnProcessor::AbortStage_(TurnStageBase& rStage)
{
    if (!m_bStageEntered)
    {
        m_completedFactionIds.clear();
        m_bHasResumeFaction = false;
        m_resumeFactionId = 0;
        return;
    }
    m_bStageEntered = false;
    m_completedFactionIds.clear();
    m_bHasResumeFaction = false;
    m_resumeFactionId = 0;
    rStage.OnExit();
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
    try
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
    catch (...)
    {
        AbortStage_(rStage);
        throw;
    }
}

StageResult_t TurnProcessor::ExecutePerFactionStage_(PerFactionTurnStage& rStage,
                                                     GameState& rGameState)
{
    try
    {
        EnsureEntered_(rStage);

        if (m_bHasResumeFaction)
        {
            bool bResumePresent = false;
            for (Faction& rFaction : rGameState.Factions())
            {
                if (rFaction.GetFactionId() == m_resumeFactionId)
                {
                    bResumePresent = true;
                    break;
                }
            }
            if (!bResumePresent)
            {
                // Resume faction was eliminated while yielded; process remaining factions.
                m_bHasResumeFaction = false;
            }
        }

        for (Faction& rFaction : rGameState.Factions())
        {
            const FactionId_t factionId = rFaction.GetFactionId();
            if (m_completedFactionIds.contains(factionId))
            {
                continue;
            }
            if (m_bHasResumeFaction && factionId != m_resumeFactionId)
            {
                continue;
            }

            const StageResult_t result = rStage.Execute(rGameState, rFaction);
            if (result == StageResult_t::Yield)
            {
                m_resumeFactionId = factionId;
                m_bHasResumeFaction = true;
                return StageResult_t::Yield;
            }

            m_completedFactionIds.insert(factionId);
            m_bHasResumeFaction = false;
        }

        CompleteStage_(rStage);
        return StageResult_t::Continue;
    }
    catch (...)
    {
        AbortStage_(rStage);
        throw;
    }
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

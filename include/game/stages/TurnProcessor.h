#pragma once

#include "game/stages/TurnStages.h"
#include "game/base/BaseTypes.h"
#include <unordered_set>
#include <string>
#include <vector>

namespace ac
{

class GameState;

class TurnProcessor
{
public:
    // Takes ownership of the stage registries and order. Throws if stageOrder is empty.
    TurnProcessor(GlobalTurnStageRegistry_t globalRegistry,
                  PerFactionTurnStageRegistry_t perFactionRegistry,
                  std::vector<std::string> stageOrder);

    // Runs stages until one yields, wrapping into the next turn; call again once the yield is
    // resolved. Throws if a whole cycle passes without one.
    void Advance(GameState& rGameState);

    // Returns the processor to stage index 0 with no entered stage / resume cursor.
    // Use after a stage throw or a no-yield poison so a later Advance can start cleanly.
    void Reset();

private:
    void CompleteStage_(TurnStageBase& rStage, GameState& rGameState);
    void AbortStage_(TurnStageBase& rStage, GameState& rGameState);
    void EnsureEntered_(TurnStageBase& rStage, GameState& rGameState);
    StageResult_t ExecuteCurrentStage_(GameState& rGameState);
    StageResult_t ExecuteGlobalStage_(GlobalTurnStage& rStage, GameState& rGameState);
    StageResult_t ExecutePerFactionStage_(PerFactionTurnStage& rStage, GameState& rGameState);

    GlobalTurnStageRegistry_t m_globalRegistry;
    PerFactionTurnStageRegistry_t m_perFactionRegistry;
    std::vector<std::string> m_stageOrder;

    size_t m_stageIndex = 0;
    // Factions done with the current per-faction stage, skipped on resume.
    // TODO: removing a faction mid-stage is undefined until the lifetime protocol covers it.
    std::unordered_set<FactionId_t> m_completedFactionIds;
    FactionId_t m_resumeFactionId = 0;
    bool m_bHasResumeFaction = false;
    bool m_bStageEntered = false;
};

} // namespace ac

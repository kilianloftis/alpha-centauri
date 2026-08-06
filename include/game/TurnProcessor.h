#pragma once

#include "game/TurnStages.h"
#include "game/faction/base/BaseTypes.h"
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

    // Runs stages until one yields, wrapping from the end of the stage order back to the
    // start (next turn cycle) as needed. Call again after the yield reason is resolved
    // (player Enter, popup closed, …) to resume the same stage/faction.
    // Throws if a full stage-order cycle completes with no yield (misconfigured order).
    void Advance(GameState& rGameState);

    // Returns the processor to stage index 0 with no entered stage / resume cursor.
    // Use after a stage throw or a no-yield poison so a later Advance can start cleanly.
    void Reset();

private:
    void CompleteStage_(TurnStageBase& rStage);
    void AbortStage_(TurnStageBase& rStage);
    void EnsureEntered_(TurnStageBase& rStage);
    StageResult_t ExecuteCurrentStage_(GameState& rGameState);
    StageResult_t ExecuteGlobalStage_(GlobalTurnStage& rStage, GameState& rGameState);
    StageResult_t ExecutePerFactionStage_(PerFactionTurnStage& rStage, GameState& rGameState);

    GlobalTurnStageRegistry_t m_globalRegistry;
    PerFactionTurnStageRegistry_t m_perFactionRegistry;
    std::vector<std::string> m_stageOrder;

    size_t m_stageIndex = 0;
    // Faction ids that have already finished (Continue) for the current per-faction stage.
    // Resume re-enters the yielded faction; completed ids are skipped. Erasing a faction
    // mid-stage loop is unsupported until the lifetime protocol defines it.
    std::unordered_set<FactionId_t> m_completedFactionIds;
    FactionId_t m_resumeFactionId = 0;
    bool m_bHasResumeFaction = false;
    bool m_bStageEntered = false;
};

} // namespace ac

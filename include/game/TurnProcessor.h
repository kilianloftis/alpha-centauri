#pragma once

#include "game/TurnStages.h"
#include "game/faction/base/BaseTypes.h"
#include <optional>
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

private:
    void CompleteStage_(TurnStageBase& rStage);
    void EnsureEntered_(TurnStageBase& rStage);
    StageResult_t ExecuteCurrentStage_(GameState& rGameState);
    StageResult_t ExecuteGlobalStage_(GlobalTurnStage& rStage, GameState& rGameState);
    StageResult_t ExecutePerFactionStage_(PerFactionTurnStage& rStage, GameState& rGameState);

    GlobalTurnStageRegistry_t m_globalRegistry;
    PerFactionTurnStageRegistry_t m_perFactionRegistry;
    std::vector<std::string> m_stageOrder;

    size_t m_stageIndex = 0;
    // When set, per-faction resume skips factions with a lower id (FactionIds are
    // monotonically allocated and Factions() stays in insertion order). If the resume
    // faction was eliminated while yielded, the first remaining faction with a greater
    // id is processed next — so later factions are not skipped.
    std::optional<FactionId_t> m_resumeFactionId;
    bool m_bStageEntered = false;
};

} // namespace ac

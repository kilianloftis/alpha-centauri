#pragma once

#include "game/TurnStages.h"
#include <string>
#include <vector>

namespace ac
{

class GameState;

class TurnProcessor
{
public:
    TurnProcessor(GlobalTurnStageRegistry_t globalRegistry,
                  PerFactionTurnStageRegistry_t perFactionRegistry,
                  std::vector<std::string> stageOrder);
    ~TurnProcessor() = default;

    void ProcessTurn(GameState& rGameState);

private:
    GlobalTurnStageRegistry_t m_globalRegistry;
    PerFactionTurnStageRegistry_t m_perFactionRegistry;
    std::vector<std::string> m_stageOrder;
};

} // namespace ac

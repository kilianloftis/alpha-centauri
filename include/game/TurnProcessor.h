#pragma once

#include "game/TurnStages.h"
#include <memory>

namespace ac
{

class GameState;

class TurnProcessor
{
public:
    TurnProcessor(TurnStageRegistry_t registry);
    ~TurnProcessor() = default;

    void ProcessTurn(int missionYear, int numFactions, GameState& rGameState);

private:
    int m_missionYear;
    TurnStageRegistry_t m_registry;
};

} // namespace ac

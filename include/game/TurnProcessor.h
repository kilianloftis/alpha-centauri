#pragma once

#include "game/TurnStages.h"
#include <memory>
#include <map>

namespace ac
{

class GameState;

using TurnStageRepeatFlags_t = std::map<TurnStage, bool>;

class TurnProcessor
{
public:
    TurnProcessor(TurnStageRegistry_t registry, TurnStageRepeatFlags_t repeatFlags);
    ~TurnProcessor() = default;

    void ProcessTurn(int missionYear, int numFactions, GameState& rGameState);

private:
    int m_missionYear;
    TurnStageRegistry_t m_registry;
    TurnStageRepeatFlags_t m_repeatFlags;
};

} // namespace ac

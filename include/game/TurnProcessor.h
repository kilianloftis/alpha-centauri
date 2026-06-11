#pragma once

#include "game/TurnStages.h"
#include <memory>
#include <map>
#include <string>
#include <vector>

namespace ac
{

class GameState;

using TurnStageRepeatFlags_t = std::map<std::string, bool>;

class TurnProcessor
{
public:
    TurnProcessor(TurnStageRegistry_t registry, TurnStageRepeatFlags_t repeatFlags, std::vector<std::string> stageOrder);
    ~TurnProcessor() = default;

    void ProcessTurn(int missionYear, int numFactions, GameState& rGameState);

private:
    int m_missionYear;
    TurnStageRegistry_t m_registry;
    TurnStageRepeatFlags_t m_repeatFlags;
    std::vector<std::string> m_stageOrder;
};

} // namespace ac

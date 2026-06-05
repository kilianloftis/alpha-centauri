#pragma once

#include "game/TurnStages.h"
#include <memory>

namespace ac
{

class TurnProcessor
{
public:
    TurnProcessor(TurnStageRegistry_t registry);
    ~TurnProcessor() = default;

    void ProcessTurn(int missionYear, int numFactions);

private:
    int m_missionYear;
    TurnStageRegistry_t m_registry;
};

} // namespace ac

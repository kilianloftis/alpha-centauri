#pragma once

#include "game/TurnStages.h"

namespace ac
{

class WorldEvents : public GlobalTurnStage
{
public:
    explicit WorldEvents(HookContext hookContext);

protected:
    StageResult_t ExecuteImpl(GameState& rGameState) override;
};

} // namespace ac

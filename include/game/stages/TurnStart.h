#pragma once

#include "game/TurnStages.h"

namespace ac
{

class TurnStart : public GlobalTurnStage
{
public:
    explicit TurnStart(HookContext hookContext);
    ~TurnStart() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState) override;
};

} // namespace ac

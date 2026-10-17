#pragma once

#include "game/stages/TurnStages.h"

namespace ac
{

class TurnEnd : public GlobalTurnStage
{
public:
    explicit TurnEnd(HookContext hookContext);
    ~TurnEnd() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState) override;
};

} // namespace ac

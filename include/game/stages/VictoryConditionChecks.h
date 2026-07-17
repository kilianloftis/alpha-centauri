#pragma once

#include "game/TurnStages.h"

namespace ac
{

class VictoryConditionChecks : public GlobalTurnStage
{
public:
    explicit VictoryConditionChecks(HookContext hookContext);
    ~VictoryConditionChecks() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState) override;
};

} // namespace ac

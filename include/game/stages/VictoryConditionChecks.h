#pragma once

#include "game/stages/TurnStages.h"

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

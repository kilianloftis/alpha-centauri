#pragma once

#include "game/TurnStages.h"

namespace ac
{

class VictoryConditionChecks : public GlobalTurnStage
{
public:
    explicit VictoryConditionChecks(std::shared_ptr<HookContext> pHookContext);
    ~VictoryConditionChecks() = default;

protected:
    void ExecuteImpl(GameState& rGameState) override;
};

} // namespace ac

#pragma once

#include "game/TurnStages.h"

namespace ac
{

class TurnEnd : public GlobalTurnStage
{
public:
    explicit TurnEnd(std::shared_ptr<HookContext> pHookContext);
    ~TurnEnd() = default;

protected:
    void ExecuteImpl(GameState& rGameState) override;
};

} // namespace ac

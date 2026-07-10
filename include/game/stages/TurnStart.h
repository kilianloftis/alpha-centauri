#pragma once

#include "game/TurnStages.h"

namespace ac
{

class TurnStart : public GlobalTurnStage
{
public:
    explicit TurnStart(std::shared_ptr<HookContext> pHookContext);
    ~TurnStart() = default;

protected:
    void ExecuteImpl(GameState& rGameState) override;
};

} // namespace ac

#pragma once

#include "game/TurnStages.h"

namespace ac
{

class WorldEvents : public GlobalTurnStage
{
public:
    explicit WorldEvents(std::shared_ptr<HookContext> pHookContext);
    ~WorldEvents() = default;

protected:
    void ExecuteImpl(GameState& rGameState) override;
};

} // namespace ac

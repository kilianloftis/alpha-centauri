#pragma once

#include "game/TurnStages.h"

namespace ac
{

class Save : public GlobalTurnStage
{
public:
    explicit Save(std::shared_ptr<HookContext> pHookContext);
    ~Save() = default;

protected:
    void ExecuteImpl(GameState& rGameState) override;
};

} // namespace ac

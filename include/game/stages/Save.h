#pragma once

#include "game/stages/TurnStages.h"

namespace ac
{

class Save : public GlobalTurnStage
{
public:
    explicit Save(HookContext hookContext);
    ~Save() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState) override;
};

} // namespace ac

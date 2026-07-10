#pragma once

#include "game/TurnStages.h"

namespace ac
{

class NewYearBegins : public GlobalTurnStage
{
public:
    explicit NewYearBegins(std::shared_ptr<HookContext> pHookContext);
    ~NewYearBegins() = default;

protected:
    void ExecuteImpl(GameState& rGameState) override;
};

} // namespace ac

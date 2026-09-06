#pragma once

#include "game/TurnStages.h"

namespace ac
{

class BaseGrowth : public PerFactionTurnStage
{
public:
    explicit BaseGrowth(HookContext hookContext);
    ~BaseGrowth() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

#pragma once

#include "game/TurnStages.h"

namespace ac
{

class MineralConversion : public PerFactionTurnStage
{
public:
    explicit MineralConversion(HookContext hookContext);
    ~MineralConversion() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

#pragma once

#include "game/TurnStages.h"

namespace ac
{

class Upkeep : public PerFactionTurnStage
{
public:
    explicit Upkeep(HookContext hookContext);
    ~Upkeep() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

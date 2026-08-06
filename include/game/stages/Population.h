#pragma once

#include "game/TurnStages.h"

namespace ac
{

class Population : public PerFactionTurnStage
{
public:
    explicit Population(HookContext hookContext);

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

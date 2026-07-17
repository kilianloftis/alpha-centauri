#pragma once

#include "game/TurnStages.h"

namespace ac
{

class BaseProduction : public PerFactionTurnStage
{
public:
    explicit BaseProduction(HookContext hookContext);
    ~BaseProduction() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

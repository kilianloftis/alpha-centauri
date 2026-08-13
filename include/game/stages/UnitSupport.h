#pragma once

#include "game/TurnStages.h"

namespace ac
{

class UnitSupport : public PerFactionTurnStage
{
public:
    explicit UnitSupport(HookContext hookContext);
    ~UnitSupport() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

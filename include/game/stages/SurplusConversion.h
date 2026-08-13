#pragma once

#include "game/TurnStages.h"

namespace ac
{

class SurplusConversion : public PerFactionTurnStage
{
public:
    explicit SurplusConversion(HookContext hookContext);
    ~SurplusConversion() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

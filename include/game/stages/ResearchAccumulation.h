#pragma once

#include "game/TurnStages.h"

namespace ac
{

class ResearchAccumulation : public PerFactionTurnStage
{
public:
    explicit ResearchAccumulation(HookContext hookContext);
    ~ResearchAccumulation() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

#pragma once

#include "game/stages/TurnStages.h"

namespace ac
{

// After Mood: commit pending social policies and clear upheavalPaid (SMAC social_upkeep).
class SocialEngineeringUpkeep : public PerFactionTurnStage
{
public:
    explicit SocialEngineeringUpkeep(HookContext hookContext);

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

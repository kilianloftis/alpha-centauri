#pragma once

#include "game/TurnStages.h"

namespace ac
{

class ResearchAccumulation : public PerFactionTurnStage
{
public:
    explicit ResearchAccumulation(std::shared_ptr<HookContext> pHookContext);
    ~ResearchAccumulation() = default;

protected:
    void ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

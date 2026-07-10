#pragma once

#include "game/TurnStages.h"

namespace ac
{

class Population : public PerFactionTurnStage
{
public:
    explicit Population(std::shared_ptr<HookContext> pHookContext);
    ~Population() = default;

protected:
    void ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

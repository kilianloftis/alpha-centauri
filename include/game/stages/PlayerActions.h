#pragma once

#include "game/TurnStages.h"

namespace ac
{

class PlayerActions : public PerFactionTurnStage
{
public:
    explicit PlayerActions(std::shared_ptr<HookContext> pHookContext);
    ~PlayerActions() = default;

protected:
    void ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

#pragma once

#include "game/TurnStages.h"

namespace ac
{

class Upkeep : public PerFactionTurnStage
{
public:
    explicit Upkeep(std::shared_ptr<HookContext> pHookContext);
    ~Upkeep() = default;

protected:
    void ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

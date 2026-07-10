#pragma once

#include "game/TurnStages.h"

namespace ac
{

class BaseProduction : public PerFactionTurnStage
{
public:
    explicit BaseProduction(std::shared_ptr<HookContext> pHookContext);
    ~BaseProduction() = default;

protected:
    void ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

#pragma once

#include "game/TurnStages.h"

namespace ac
{

class VictoryConditionChecks : public TurnStageBase
{
public:
    VictoryConditionChecks(std::shared_ptr<HookContext> hookContext);
    ~VictoryConditionChecks() = default;

    void Execute_(GameState* pGameState, Faction* pFaction = nullptr) override;
};

} // namespace ac

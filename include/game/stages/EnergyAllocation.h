#pragma once

#include "game/TurnStages.h"

namespace ac
{

class EnergyAllocation : public TurnStageBase
{
public:
    EnergyAllocation(std::shared_ptr<HookContext> hookContext);
    ~EnergyAllocation() = default;

    void Execute_(GameState* pGameState) override;
};

} // namespace ac

#pragma once

#include "game/TurnStages.h"

namespace ac
{

class PopCompositionCalculator;

class Population : public TurnStageBase
{
public:
    Population(std::shared_ptr<HookContext> hookContext);
    ~Population() = default;

private:
    void Execute_(GameState* pGameState, Faction* pFaction = nullptr) override;
};

} // namespace ac

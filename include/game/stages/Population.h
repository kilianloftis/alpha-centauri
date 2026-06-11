#pragma once

#include "game/TurnStages.h"

namespace ac
{

class GrowthCalculator;

class Population : public TurnStageBase
{
public:
    Population(std::shared_ptr<HookContext> hookContext, GrowthCalculator* pGrowthCalculator);
    ~Population() = default;

private:
    void Execute_(GameState* pGameState, Faction* pFaction = nullptr) override;

    GrowthCalculator* m_pGrowthCalculator;
};

} // namespace ac

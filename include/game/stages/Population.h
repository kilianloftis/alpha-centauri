#pragma once

#include "game/TurnStages.h"

namespace ac
{

class PopCompositionCalculator;

class Population : public TurnStageBase
{
public:
    Population(std::shared_ptr<HookContext> hookContext, PopCompositionCalculator* pCalculator);
    ~Population() = default;

    void Execute_(GameState* pGameState, Faction* pFaction = nullptr) override;

private:
    PopCompositionCalculator* m_pCalculator;
};

} // namespace ac

#pragma once

#include "game/TurnStages.h"

namespace ac
{

class ResearchAccumulation : public TurnStageBase
{
public:
    ResearchAccumulation(std::shared_ptr<HookContext> hookContext);
    ~ResearchAccumulation() = default;

    void Execute_(GameState* pGameState) override;
};

} // namespace ac

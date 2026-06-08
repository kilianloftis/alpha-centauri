#pragma once

#include "game/TurnStages.h"

namespace ac
{

class BaseProduction : public TurnStageBase
{
public:
    BaseProduction(std::shared_ptr<HookContext> hookContext);
    ~BaseProduction() = default;

    void Execute_(GameState* pGameState) override;
};

} // namespace ac

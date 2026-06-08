#pragma once

#include "game/TurnStages.h"

namespace ac
{

class IncomeCollection : public TurnStageBase
{
public:
    IncomeCollection(std::shared_ptr<HookContext> hookContext);
    ~IncomeCollection() = default;

    void Execute_(GameState* pGameState) override;
};

} // namespace ac

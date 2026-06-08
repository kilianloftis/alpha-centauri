#pragma once

#include "game/TurnStages.h"

namespace ac
{

class TurnStart : public TurnStageBase
{
public:
    TurnStart(std::shared_ptr<HookContext> hookContext);
    ~TurnStart() = default;

    void Execute_(GameState* pGameState) override;
};

} // namespace ac

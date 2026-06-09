#pragma once

#include "game/TurnStages.h"

namespace ac
{

class Upkeep : public TurnStageBase
{
public:
    Upkeep(std::shared_ptr<HookContext> hookContext);
    ~Upkeep() = default;

    void Execute_(GameState* pGameState, Faction* pFaction = nullptr) override;
};

} // namespace ac

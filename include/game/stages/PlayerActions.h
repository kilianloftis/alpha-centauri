#pragma once

#include "game/TurnStages.h"

namespace ac
{

class PlayerActions : public TurnStageBase
{
public:
    PlayerActions(std::shared_ptr<HookContext> hookContext);
    ~PlayerActions() = default;

    void Execute_(GameState* pGameState, Faction* pFaction = nullptr) override;
};

} // namespace ac

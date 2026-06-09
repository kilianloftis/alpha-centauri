#pragma once

#include "game/TurnStages.h"

namespace ac
{

class WorldEvents : public TurnStageBase
{
public:
    WorldEvents(std::shared_ptr<HookContext> hookContext);
    ~WorldEvents() = default;

    void Execute_(GameState* pGameState, Faction* pFaction = nullptr) override;
};

} // namespace ac

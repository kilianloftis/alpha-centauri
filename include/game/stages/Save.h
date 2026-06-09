#pragma once

#include "game/TurnStages.h"

namespace ac
{

class Save : public TurnStageBase
{
public:
    Save(std::shared_ptr<HookContext> hookContext);
    ~Save() = default;

    void Execute_(GameState* pGameState, Faction* pFaction = nullptr) override;
};

} // namespace ac

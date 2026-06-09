#pragma once

#include "game/TurnStages.h"

namespace ac
{

class NewYearBegins : public TurnStageBase
{
public:
    NewYearBegins(std::shared_ptr<HookContext> hookContext);
    ~NewYearBegins() = default;

    void Execute_(GameState* pGameState, Faction* pFaction = nullptr) override;
};

} // namespace ac

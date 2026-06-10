#pragma once

#include "game/TurnStages.h"

namespace ac
{

class ResourceCollection : public TurnStageBase
{
public:
    ResourceCollection(std::shared_ptr<HookContext> hookContext);
    ~ResourceCollection() = default;

    void Execute_(GameState* pGameState, Faction* pFaction = nullptr) override;
};

} // namespace ac

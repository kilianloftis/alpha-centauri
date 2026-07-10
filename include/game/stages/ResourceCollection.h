#pragma once

#include "game/TurnStages.h"

namespace ac
{

class ResourceCollection : public PerFactionTurnStage
{
public:
    explicit ResourceCollection(std::shared_ptr<HookContext> pHookContext);
    ~ResourceCollection() = default;

protected:
    void ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

#pragma once

#include "game/TurnStages.h"

namespace ac
{

class ResourceCollection : public PerFactionTurnStage
{
public:
    explicit ResourceCollection(HookContext hookContext);
    ~ResourceCollection() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

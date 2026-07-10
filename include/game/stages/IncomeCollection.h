#pragma once

#include "game/TurnStages.h"

namespace ac
{

class IncomeCollection : public PerFactionTurnStage
{
public:
    explicit IncomeCollection(std::shared_ptr<HookContext> pHookContext);
    ~IncomeCollection() = default;

protected:
    void ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

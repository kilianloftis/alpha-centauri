#pragma once

#include "game/TurnStages.h"

namespace ac
{

class IncomeCollection : public PerFactionTurnStage
{
public:
    explicit IncomeCollection(HookContext hookContext);
    ~IncomeCollection() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
};

} // namespace ac

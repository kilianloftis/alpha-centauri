#pragma once

#include "game/stages/BaseProduction.h"

namespace ac
{

// After PlayerActions: complete funded queues without banking leftovers or clearing
// same-turn abandon deferrals (TryCompleteReady with bNewTurn=false).
class PostActionsProduction : public BaseProduction
{
public:
    explicit PostActionsProduction(HookContext hookContext);

protected:
    ProductionApplyResult_t TickBase_(BaseManager& rBase) override;
};

} // namespace ac

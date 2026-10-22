#pragma once

#include "game/stages/BaseProduction.h"

namespace ac
{

// After PlayerActions: complete funded queues without banking leftovers or clearing this
// turn's abandon deferrals, then validate.
class PostActionsProduction : public BaseProduction
{
public:
    explicit PostActionsProduction(HookContext hookContext);

protected:
    ProductionApplyResult_t TickBase_(BaseManager& rBase) override;
};

} // namespace ac

#include "game/stages/PostActionsProduction.h"

#include "game/stages/TurnStageRegistrar.h"

#include <utility>

namespace ac
{

namespace { TurnStageRegistrar<PostActionsProduction> g_registrar("PostActionsProduction"); }

PostActionsProduction::PostActionsProduction(HookContext hookContext)
    : BaseProduction(std::move(hookContext))
{
}

ProductionApplyResult_t PostActionsProduction::TickBase_(BaseManager& rBase)
{
    return rBase.TryCompleteReadyProduction();
}

} // namespace ac

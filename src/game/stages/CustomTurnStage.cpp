#include "game/stages/CustomTurnStage.h"
#include <stdexcept>

namespace ac
{

namespace
{
void RequireAtLeastOneHook(const HookContext* pHookContext, const std::string& name)
{
    if (!pHookContext || (!pHookContext->HasReplaceHooks() && !pHookContext->HasPreHooks() && !pHookContext->HasPostHooks()))
    {
        throw std::runtime_error("Custom turn stage '" + name + "' requires at least one hook to be defined");
    }
}
} // namespace

CustomGlobalTurnStage::CustomGlobalTurnStage(std::shared_ptr<HookContext> pHookContext, const std::string& name)
    : GlobalTurnStage(pHookContext)
    , m_name(name)
{
    RequireAtLeastOneHook(pHookContext.get(), m_name);
}

CustomPerFactionTurnStage::CustomPerFactionTurnStage(std::shared_ptr<HookContext> pHookContext, const std::string& name)
    : PerFactionTurnStage(pHookContext)
    , m_name(name)
{
    RequireAtLeastOneHook(pHookContext.get(), m_name);
}

} // namespace ac

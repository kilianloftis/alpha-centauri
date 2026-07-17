#include "game/stages/CustomTurnStage.h"
#include <stdexcept>
#include <utility>

namespace ac
{

namespace
{
void RequireAtLeastOneHook(const HookContext& rHookContext, const std::string& name)
{
    if (!rHookContext.HasReplaceHooks() && !rHookContext.HasPreHooks() && !rHookContext.HasPostHooks())
    {
        throw std::runtime_error("Custom turn stage '" + name + "' requires at least one hook to be defined");
    }
}
} // namespace

CustomGlobalTurnStage::CustomGlobalTurnStage(HookContext hookContext, const std::string& name)
    : GlobalTurnStage(std::move(hookContext))
    , m_name(name)
{
    RequireAtLeastOneHook(m_hookContext, m_name);
}

CustomPerFactionTurnStage::CustomPerFactionTurnStage(HookContext hookContext, const std::string& name)
    : PerFactionTurnStage(std::move(hookContext))
    , m_name(name)
{
    RequireAtLeastOneHook(m_hookContext, m_name);
}

} // namespace ac

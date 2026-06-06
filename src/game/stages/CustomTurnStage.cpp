#include "game/stages/CustomTurnStage.h"
#include <iostream>

namespace ac
{

CustomTurnStage::CustomTurnStage(std::shared_ptr<HookContext> hookContext, const std::string& name)
    : TurnStageBase(hookContext)
    , m_name(name)
{
    if (!m_pHookContext || (!m_pHookContext->HasReplaceHooks() && !m_pHookContext->HasPreHooks() && !m_pHookContext->HasPostHooks()))
    {
        throw std::runtime_error("CustomTurnStage '" + m_name + "' requires at least one hook to be defined");
    }
}
} // namespace ac

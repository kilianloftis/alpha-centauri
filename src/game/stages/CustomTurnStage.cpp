#include "game/stages/CustomTurnStage.h"
#include <iostream>

namespace ac
{

CustomTurnStage::CustomTurnStage(std::shared_ptr<HookContext> hookContext, const std::string& name)
    : TurnStageBase(hookContext)
    , m_name(name)
{
    if (!m_hookContext || (!m_hookContext->HasReplaceHooks() && !m_hookContext->HasPreHooks() && !m_hookContext->HasPostHooks()))
    {
        throw std::runtime_error("CustomTurnStage '" + m_name + "' requires at least one hook to be defined");
    }
}
} // namespace ac

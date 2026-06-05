#include "game/HookContext.h"
#include <iostream>

namespace ac
{

HookContext::HookContext()
{
}

void HookContext::AddPreHook(const Hook& hook)
{
    m_preHooks.push_back(hook);
}

void HookContext::AddPostHook(const Hook& hook)
{
    m_postHooks.push_back(hook);
}

void HookContext::AddReplaceHook(const Hook& hook)
{
    m_replaceHooks.push_back(hook);
}

void HookContext::ExecutePreHooks()
{
    for (const auto& hook : m_preHooks)
    {
        std::cout << "  Executing pre hook from mod: " << hook.mod_id << "\n";
        if (hook.callback)
        {
            hook.callback();
        }
    }
}

void HookContext::ExecutePostHooks()
{
    for (const auto& hook : m_postHooks)
    {
        std::cout << "  Executing post hook from mod: " << hook.mod_id << "\n";
        if (hook.callback)
        {
            hook.callback();
        }
    }
}

void HookContext::ExecuteReplaceHooks()
{
    for (const auto& hook : m_replaceHooks)
    {
        std::cout << "  Executing replace hook from mod: " << hook.mod_id << "\n";
        if (hook.callback)
        {
            hook.callback();
        }
    }
}

bool HookContext::HasReplaceHooks() const
{
    return !m_replaceHooks.empty();
}

bool HookContext::HasPreHooks() const
{
    return !m_preHooks.empty();
}

bool HookContext::HasPostHooks() const
{
    return !m_postHooks.empty();
}

} // namespace ac

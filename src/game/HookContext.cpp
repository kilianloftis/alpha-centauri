#include "game/HookContext.h"

namespace ac
{

void HookContext::AddPreHook(const Hook_t& hook)
{
    m_preHooks.push_back(hook);
}

void HookContext::AddPostHook(const Hook_t& hook)
{
    m_postHooks.push_back(hook);
}

void HookContext::AddReplaceHook(const Hook_t& hook)
{
    m_replaceHooks.push_back(hook);
}

void HookContext::ExecutePreHooks()
{
    for (const auto& hook : m_preHooks)
    {
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
        if (hook.callback)
        {
            hook.callback();
        }
    }
}

bool HookContext::HasCallable_(const std::vector<Hook_t>& rHooks)
{
    for (const Hook_t& rHook : rHooks)
    {
        if (rHook.callback)
        {
            return true;
        }
    }
    return false;
}

bool HookContext::HasReplaceHooks() const
{
    return HasCallable_(m_replaceHooks);
}

bool HookContext::HasPreHooks() const
{
    return !m_preHooks.empty();
}

bool HookContext::HasPostHooks() const
{
    return !m_postHooks.empty();
}

bool HookContext::HasCallableHook() const
{
    return HasCallable_(m_preHooks) || HasCallable_(m_postHooks) || HasCallable_(m_replaceHooks);
}

} // namespace ac

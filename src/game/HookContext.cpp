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

void HookContext::ExecutePreHooks(const HookArgs_t& rArgs)
{
    for (const Hook_t& rHook : m_preHooks)
    {
        if (rHook.callback)
        {
            rHook.callback(rArgs);
        }
    }
}

void HookContext::ExecutePostHooks(const HookArgs_t& rArgs)
{
    for (const Hook_t& rHook : m_postHooks)
    {
        if (rHook.callback)
        {
            rHook.callback(rArgs);
        }
    }
}

void HookContext::ExecuteReplaceHooks(const HookArgs_t& rArgs)
{
    for (const Hook_t& rHook : m_replaceHooks)
    {
        if (rHook.callback)
        {
            rHook.callback(rArgs);
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

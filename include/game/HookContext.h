#pragma once

#include <functional>
#include <string>
#include <vector>

namespace ac
{

struct Hook_t
{
    std::string modId;
    std::string scriptPath;
    // Package 16 may widen this to return StageResult_t; until then replace hooks cannot Yield.
    std::function<void()> callback;
};

class HookContext
{
public:
    HookContext() = default;

    void AddPreHook(const Hook_t& hook);
    void AddPostHook(const Hook_t& hook);
    void AddReplaceHook(const Hook_t& hook);

    void ExecutePreHooks();
    void ExecutePostHooks();
    void ExecuteReplaceHooks();

    // True only when at least one replace hook has a callable callback. List presence alone
    // must not suppress built-in ExecuteImpl (unbound replace entries are inert).
    bool HasReplaceHooks() const;
    bool HasPreHooks() const;
    bool HasPostHooks() const;

    // True when any hook list contains a callable callback (Custom* construction guard).
    bool HasCallableHook() const;

private:
    static bool HasCallable_(const std::vector<Hook_t>& rHooks);

    std::vector<Hook_t> m_preHooks;
    std::vector<Hook_t> m_postHooks;
    std::vector<Hook_t> m_replaceHooks;
};

} // namespace ac

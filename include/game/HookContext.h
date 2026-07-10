#pragma once

#include <string>
#include <vector>
#include <functional>

namespace ac
{

struct Hook_t
{
    std::string modId;
    std::string scriptPath;
    std::function<void()> callback;
};

class HookContext
{
public:
    HookContext();
    ~HookContext() = default;

    void AddPreHook(const Hook_t& hook);
    void AddPostHook(const Hook_t& hook);
    void AddReplaceHook(const Hook_t& hook);

    void ExecutePreHooks();
    void ExecutePostHooks();
    void ExecuteReplaceHooks();

    bool HasReplaceHooks() const;
    bool HasPreHooks() const;
    bool HasPostHooks() const;

private:
    std::vector<Hook_t> m_preHooks;
    std::vector<Hook_t> m_postHooks;
    std::vector<Hook_t> m_replaceHooks;
};

} // namespace ac

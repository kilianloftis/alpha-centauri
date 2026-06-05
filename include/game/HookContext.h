#pragma once

#include <string>
#include <vector>
#include <functional>

namespace ac
{

struct Hook
{
    std::string mod_id;
    std::string script_path;
    std::function<void()> callback;
};

class HookContext
{
public:
    HookContext();
    ~HookContext() = default;

    void AddPreHook(const Hook& hook);
    void AddPostHook(const Hook& hook);
    void AddReplaceHook(const Hook& hook);

    void ExecutePreHooks();
    void ExecutePostHooks();
    void ExecuteReplaceHooks();

    bool HasReplaceHooks() const;
    bool HasPreHooks() const;
    bool HasPostHooks() const;

private:
    std::vector<Hook> m_preHooks;
    std::vector<Hook> m_postHooks;
    std::vector<Hook> m_replaceHooks;
};

} // namespace ac

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace ac
{

class Faction;
class GameState;

// What a hook is handed when it fires. Without this a hook could only act through state it
// captured at construction, which no config-declared hook has — the seam existed but could not
// host a consumer.
struct HookArgs_t
{
    // The stage this hook is attached to, so one handler can serve several stages.
    std::string stageId;
    GameState& rGameState;
    // The faction being processed, for a per-faction stage. Null for a global stage — a global
    // stage has no faction, and handing it an arbitrary one would be a lie.
    Faction* pFaction = nullptr;
};

struct Hook_t
{
    std::string modId;
    std::string scriptPath;
    // Widening this to return StageResult_t (so a replace hook could Yield) is still open.
    std::function<void(const HookArgs_t&)> callback;
};

class HookContext
{
public:
    HookContext() = default;

    // Named at construction from config so hooks can identify the stage that fired them.
    explicit HookContext(std::string stageId)
        : m_stageId(std::move(stageId))
    {
    }

    const std::string& GetStageId() const { return m_stageId; }

    void AddPreHook(const Hook_t& hook);
    void AddPostHook(const Hook_t& hook);
    void AddReplaceHook(const Hook_t& hook);

    void ExecutePreHooks(const HookArgs_t& rArgs);
    void ExecutePostHooks(const HookArgs_t& rArgs);
    void ExecuteReplaceHooks(const HookArgs_t& rArgs);

    // True only when at least one replace hook has a callable callback. List presence alone
    // must not suppress built-in ExecuteImpl (unbound replace entries are inert).
    bool HasReplaceHooks() const;

    // True when any hook list contains a callable callback (Custom* construction guard).
    bool HasCallableHook() const;

private:
    static bool HasCallable_(const std::vector<Hook_t>& rHooks);

    std::string m_stageId;
    std::vector<Hook_t> m_preHooks;
    std::vector<Hook_t> m_postHooks;
    std::vector<Hook_t> m_replaceHooks;
};

} // namespace ac

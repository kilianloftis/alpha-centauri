#pragma once

#include "game/HookContext.h"
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace ac
{

class GameState;
class Faction;

// Result of a single stage Execute call. Yield pauses turn processing; the next Advance()
// re-enters the same stage (and, for per-faction stages, the same faction).
enum class StageResult_t
{
    Continue, // stage finished; advance to next stage / faction
    Yield     // pause turn; re-enter this stage on next Advance()
};

// Shared hook lifecycle for every turn stage. Carries no Execute contract of its own:
// concrete stages implement exactly one of GlobalTurnStage or PerFactionTurnStage below,
// so a stage never receives a parameter it cannot use.
class TurnStageBase
{
public:
    explicit TurnStageBase(HookContext hookContext)
    : m_hookContext(std::move(hookContext))
    {}

    virtual ~TurnStageBase() = default;

    void OnEnter(GameState& rGameState)
    {
        m_hookContext.ExecutePreHooks(MakeArgs_(rGameState, nullptr));
        OnEnterImpl();
    }

    void OnExit(GameState& rGameState)
    {
        OnExitImpl();
        m_hookContext.ExecutePostHooks(MakeArgs_(rGameState, nullptr));
    }

protected:
    // Skip ExecuteImpl only when a replace hook is actually callable.
    bool HasReplaceHooks() const
    {
        return m_hookContext.HasReplaceHooks();
    }

    void ExecuteReplaceHooks(GameState& rGameState, Faction* pFaction)
    {
        m_hookContext.ExecuteReplaceHooks(MakeArgs_(rGameState, pFaction));
    }

    HookArgs_t MakeArgs_(GameState& rGameState, Faction* pFaction) const
    {
        return HookArgs_t{m_hookContext.GetStageId(), rGameState, pFaction};
    }

    virtual void OnEnterImpl() {}
    virtual void OnExitImpl() {}

    HookContext m_hookContext;
};

// A stage that runs once per turn, independent of any faction (e.g. TurnStart, Save).
class GlobalTurnStage : public TurnStageBase
{
public:
    using TurnStageBase::TurnStageBase;

    StageResult_t Execute(GameState& rGameState)
    {
        if (HasReplaceHooks())
        {
            ExecuteReplaceHooks(rGameState, nullptr);
            return StageResult_t::Continue;
        }
        return ExecuteImpl(rGameState);
    }

protected:
    virtual StageResult_t ExecuteImpl(GameState& rGameState) = 0;
};

// A stage that runs once per faction per turn (e.g. IncomeCollection, BaseProduction).
class PerFactionTurnStage : public TurnStageBase
{
public:
    using TurnStageBase::TurnStageBase;

    StageResult_t Execute(GameState& rGameState, Faction& rFaction)
    {
        if (HasReplaceHooks())
        {
            ExecuteReplaceHooks(rGameState, &rFaction);
            return StageResult_t::Continue;
        }
        return ExecuteImpl(rGameState, rFaction);
    }

protected:
    virtual StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) = 0;
};

using GlobalTurnStageRegistry_t = std::map<std::string, std::unique_ptr<GlobalTurnStage>>;
using PerFactionTurnStageRegistry_t = std::map<std::string, std::unique_ptr<PerFactionTurnStage>>;

} // namespace ac

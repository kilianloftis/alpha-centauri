#pragma once

#include "game/HookContext.h"
#include <map>
#include <memory>
#include <string>

namespace ac
{

class GameState;
class Faction;

// Shared hook lifecycle for every turn stage. Carries no Execute contract of its own:
// concrete stages implement exactly one of GlobalTurnStage or PerFactionTurnStage below,
// so a stage never receives a parameter it cannot use.
class TurnStageBase
{
public:
    explicit TurnStageBase(std::shared_ptr<HookContext> pHookContext)
    : m_pHookContext(pHookContext)
    {}

    virtual ~TurnStageBase() = default;

    void OnEnter()
    {
        if (m_pHookContext)
        {
            m_pHookContext->ExecutePreHooks();
        }
    }

    void OnExit()
    {
        if (m_pHookContext)
        {
            m_pHookContext->ExecutePostHooks();
        }
    }

protected:
    bool HasReplaceHooks() const
    {
        return m_pHookContext && m_pHookContext->HasReplaceHooks();
    }

    void ExecuteReplaceHooks()
    {
        m_pHookContext->ExecuteReplaceHooks();
    }

    std::shared_ptr<HookContext> m_pHookContext;
};

// A stage that runs once per turn, independent of any faction (e.g. TurnStart, Save).
class GlobalTurnStage : public TurnStageBase
{
public:
    using TurnStageBase::TurnStageBase;

    void Execute(GameState& rGameState)
    {
        if (HasReplaceHooks())
        {
            ExecuteReplaceHooks();
        }
        else
        {
            ExecuteImpl(rGameState);
        }
    }

protected:
    virtual void ExecuteImpl(GameState& rGameState) = 0;
};

// A stage that runs once per faction per turn (e.g. IncomeCollection, BaseProduction).
class PerFactionTurnStage : public TurnStageBase
{
public:
    using TurnStageBase::TurnStageBase;

    void Execute(GameState& rGameState, Faction& rFaction)
    {
        if (HasReplaceHooks())
        {
            ExecuteReplaceHooks();
        }
        else
        {
            ExecuteImpl(rGameState, rFaction);
        }
    }

protected:
    virtual void ExecuteImpl(GameState& rGameState, Faction& rFaction) = 0;
};

using GlobalTurnStageRegistry_t = std::map<std::string, std::unique_ptr<GlobalTurnStage>>;
using PerFactionTurnStageRegistry_t = std::map<std::string, std::unique_ptr<PerFactionTurnStage>>;

} // namespace ac

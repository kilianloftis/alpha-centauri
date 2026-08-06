#pragma once

#include "game/TurnStages.h"
#include <string>

namespace ac
{

// Mod-defined stages are generic: whether they repeat per faction is a config property
// (turn_stages.json's repeatForEachFaction), not something known from a C++ type, so
// unlike the built-in stages this one comes in both a global and a per-faction flavor.
// Both are driven entirely by hooks and require at least one callable callback, or they
// would silently do nothing. Script-path hooks are rejected at config load until package 16
// binds a script runtime.

class CustomGlobalTurnStage : public GlobalTurnStage
{
public:
    CustomGlobalTurnStage(HookContext hookContext, const std::string& name);

protected:
    StageResult_t ExecuteImpl(GameState& rGameState) override
    {
        (void)rGameState;
        return StageResult_t::Continue;
    }

private:
    std::string m_name;
};

class CustomPerFactionTurnStage : public PerFactionTurnStage
{
public:
    CustomPerFactionTurnStage(HookContext hookContext, const std::string& name);

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override
    {
        (void)rGameState;
        (void)rFaction;
        return StageResult_t::Continue;
    }

private:
    std::string m_name;
};

} // namespace ac

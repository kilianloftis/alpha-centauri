#pragma once

#include "game/stages/TurnStages.h"
#include <string>

namespace ac
{

// Mod-defined stages, global or per faction as turn_stages.json says, driven by hooks; at least
// one callable callback is required.
// TODO: script-path hooks are rejected at load until a script runtime exists.

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

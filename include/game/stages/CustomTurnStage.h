#pragma once

#include "game/TurnStages.h"
#include "game/HookContext.h"
#include <string>

namespace ac
{

// Mod-defined stages are generic: whether they repeat per faction is a config property
// (turn_stages.json's repeat_for_each_faction), not something known from a C++ type, so
// unlike the built-in stages this one comes in both a global and a per-faction flavor.
// Both are driven entirely by hooks (a mod script supplies the replace hook) and both
// require at least one hook to be configured, or they would silently do nothing.

class CustomGlobalTurnStage : public GlobalTurnStage
{
public:
    CustomGlobalTurnStage(std::shared_ptr<HookContext> pHookContext, const std::string& name);
    ~CustomGlobalTurnStage() = default;

protected:
    void ExecuteImpl(GameState& rGameState) override { (void)rGameState; }

private:
    std::string m_name;
};

class CustomPerFactionTurnStage : public PerFactionTurnStage
{
public:
    CustomPerFactionTurnStage(std::shared_ptr<HookContext> pHookContext, const std::string& name);
    ~CustomPerFactionTurnStage() = default;

protected:
    void ExecuteImpl(GameState& rGameState, Faction& rFaction) override { (void)rGameState; (void)rFaction; }

private:
    std::string m_name;
};

} // namespace ac

#pragma once

#include "game/HookContext.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

struct TurnStageConfig_t
{
    std::string id;
    std::string name;
    std::string description;
    bool bRepeatForEachFaction = false;
    HookContext hookContext;
};

class TurnStageConfigParser
{
public:
    TurnStageConfigParser() = default;

    // Throws on duplicate stage ids, unbound scriptPath hooks, or empty hook objects.
    std::vector<TurnStageConfig_t> ParseConfig(const std::string& configPath);

private:
    TurnStageConfig_t ParseStageConfig_(const nlohmann::json& stageJson);
    void ParseHooks_(const nlohmann::json& hooksJson, HookContext& rHookContext);
    static Hook_t ParseHook_(const nlohmann::json& hookJson);
};

} // namespace ac

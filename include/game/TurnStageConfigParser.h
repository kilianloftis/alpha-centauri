#pragma once

#include "game/TurnStages.h"
#include "game/HookContext.h"
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace ac
{

struct TurnStageConfig_t
{
    std::string id;
    std::string name;
    std::string description;
    bool repeat_for_each_faction;
    HookContext hookContext;
};

class TurnStageConfigParser
{
public:
    TurnStageConfigParser();
    ~TurnStageConfigParser() = default;

    std::vector<TurnStageConfig_t> ParseConfig(const std::string& configPath);

private:
    TurnStageConfig_t ParseStageConfig(const nlohmann::json& stageJson);
    void ParseHooks(const nlohmann::json& hooksJson, HookContext& rHookContext);
};

} // namespace ac

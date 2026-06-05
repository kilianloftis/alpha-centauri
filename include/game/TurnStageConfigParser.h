#pragma once

#include "game/TurnStages.h"
#include "game/HookContext.h"
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace ac
{

struct TurnStageConfig
{
    std::string id;
    std::string name;
    std::string description;
    bool repeat_for_each_faction;
    std::shared_ptr<HookContext> hookContext;
};

class TurnStageConfigParser
{
public:
    TurnStageConfigParser();
    ~TurnStageConfigParser() = default;

    std::vector<TurnStageConfig> ParseConfig(const std::string& configPath);

private:
    TurnStageConfig ParseStageConfig(const nlohmann::json& stageJson);
    void ParseHooks(const nlohmann::json& hooksJson, std::shared_ptr<HookContext> hookContext);
};

} // namespace ac

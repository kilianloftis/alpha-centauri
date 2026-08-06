#include "game/TurnStageConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include <stdexcept>
#include <unordered_set>

using json = nlohmann::json;

namespace ac
{

Hook_t TurnStageConfigParser::ParseHook_(const nlohmann::json& hookJson)
{
    if (!hookJson.is_object())
    {
        throw std::runtime_error("Turn stage hook entry must be a JSON object");
    }

    Hook_t hook;
    hook.modId = hookJson.value("modId", "");
    hook.scriptPath = hookJson.value("scriptPath", "");

    if (!hook.scriptPath.empty())
    {
        throw std::runtime_error(
            "Turn stage hook names scriptPath '" + hook.scriptPath
            + "' but script loading is not available; remove the hook or bind a callback in C++");
    }
    if (hook.modId.empty())
    {
        throw std::runtime_error(
            "Turn stage hook must provide modId; empty hooks are rejected");
    }
    return hook;
}

std::vector<TurnStageConfig_t> TurnStageConfigParser::ParseConfig(const std::string& configPath)
{
    std::vector<TurnStageConfig_t> stages = JsonConfigLoader::LoadFile<TurnStageConfig_t>(
        configPath, "turn stage",
        [this](const nlohmann::json& rJson) { return ParseStageConfig(rJson); });

    std::unordered_set<std::string> seenIds;
    for (const TurnStageConfig_t& rConfig : stages)
    {
        if (!seenIds.insert(rConfig.id).second)
        {
            throw std::runtime_error("Duplicate turn stage id '" + rConfig.id + "' in " + configPath);
        }
    }
    return stages;
}

TurnStageConfig_t TurnStageConfigParser::ParseStageConfig(const nlohmann::json& stageJson)
{
    TurnStageConfig_t config;
    config.id = ConfigFields::ParseId(stageJson);
    config.name = ConfigFields::ParseName(stageJson, config.id);
    config.description = stageJson.value("description", "");

    if (!stageJson.contains("repeatForEachFaction"))
    {
        throw std::runtime_error(
            "Turn stage '" + config.id + "' is missing required key 'repeatForEachFaction'");
    }
    config.bRepeatForEachFaction = stageJson.at("repeatForEachFaction").get<bool>();

    if (stageJson.contains("hooks"))
    {
        ParseHooks(stageJson.at("hooks"), config.hookContext);
    }

    return config;
}

void TurnStageConfigParser::ParseHooks(const nlohmann::json& hooksJson, HookContext& rHookContext)
{
    if (!hooksJson.is_object())
    {
        throw std::runtime_error("Turn stage hooks must be a JSON object");
    }

    for (const auto& hookJson : hooksJson.value("pre", json::array()))
    {
        rHookContext.AddPreHook(ParseHook_(hookJson));
    }
    for (const auto& hookJson : hooksJson.value("post", json::array()))
    {
        rHookContext.AddPostHook(ParseHook_(hookJson));
    }
    for (const auto& hookJson : hooksJson.value("replace", json::array()))
    {
        rHookContext.AddReplaceHook(ParseHook_(hookJson));
    }
}

} // namespace ac

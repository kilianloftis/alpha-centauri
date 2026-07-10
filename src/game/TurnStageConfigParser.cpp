#include "game/TurnStageConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace ac
{

TurnStageConfigParser::TurnStageConfigParser()
{
}

std::vector<TurnStageConfig_t> TurnStageConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<TurnStageConfig_t>(
        configPath, "turn stage",
        [this](const nlohmann::json& rJson) { return ParseStageConfig(rJson); });
}

TurnStageConfig_t TurnStageConfigParser::ParseStageConfig(const nlohmann::json& stageJson)
{
    TurnStageConfig_t config;
    config.id = ConfigFields::ParseId(stageJson);
    config.name = ConfigFields::ParseName(stageJson, config.id);
    config.description = stageJson.value("description", "");
    config.repeat_for_each_faction = stageJson.value("repeat_for_each_faction", false);

    config.hookContext = std::make_shared<HookContext>();

    if (stageJson.contains("hooks"))
    {
        json hooksJson = stageJson["hooks"];
        ParseHooks(hooksJson, config.hookContext);
    }

    return config;
}

void TurnStageConfigParser::ParseHooks(const nlohmann::json& hooksJson, std::shared_ptr<HookContext> hookContext)
{
    for (const auto& hookId : hooksJson.value("pre", json::array()))
    {
        Hook_t hook;
        hook.modId = hookId.value("mod_id", "");
        hook.scriptPath = hookId.value("script_path", "");
        hookContext->AddPreHook(hook);
    }
    for (const auto& hookId : hooksJson.value("post", json::array()))
    {
        Hook_t hook;
        hook.modId = hookId.value("mod_id", "");
        hook.scriptPath = hookId.value("script_path", "");
        hookContext->AddPostHook(hook);
    }
    for (const auto& hookId : hooksJson.value("replace", json::array()))
    {
        Hook_t hook;
        hook.modId = hookId.value("mod_id", "");
        hook.scriptPath = hookId.value("script_path", "");
        hookContext->AddReplaceHook(hook);
    }
}

} // namespace ac

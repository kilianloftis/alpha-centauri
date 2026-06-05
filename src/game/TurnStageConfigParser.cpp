#include "game/TurnStageConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace ac
{

TurnStageConfigParser::TurnStageConfigParser()
{
}

std::vector<TurnStageConfig> TurnStageConfigParser::ParseConfig(const std::string& configPath)
{
    std::cout << "Loading turn stage configuration from: " << configPath << "\n";
    
    std::vector<TurnStageConfig> configs;
    
    try
    {
        std::ifstream configFile(configPath);
        if (!configFile.is_open())
        {
            std::cout << "Warning: Could not open " << configPath << "\n";
            return configs;
        }
        
        json configJson;
        configFile >> configJson;
        
        if (!configJson.is_array())
        {
            std::cout << "Error: Expected array of turn stages in config\n";
            return configs;
        }
        
        for (const auto& stageJson : configJson)
        {
            TurnStageConfig config = ParseStageConfig(stageJson);
            configs.push_back(config);
        }
        
        std::cout << "Loaded " << configs.size() << " turn stage configurations\n";
        return configs;
        
    }
    catch (const std::exception& e)
    {
        std::cout << "Error loading turn stage config: " << e.what() << "\n";
        return configs;
    }
}

TurnStageConfig TurnStageConfigParser::ParseStageConfig(const nlohmann::json& stageJson)
{
    TurnStageConfig config;
    config.id = stageJson["id"];
    config.name = stageJson.value("name", config.id);
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
        Hook hook;
        hook.mod_id = hookId.value("mod_id", "");
        hook.script_path = hookId.value("script_path", "");
        hookContext->AddPreHook(hook);
    }
    for (const auto& hookId : hooksJson.value("post", json::array()))
    {
        Hook hook;
        hook.mod_id = hookId.value("mod_id", "");
        hook.script_path = hookId.value("script_path", "");
        hookContext->AddPostHook(hook);
    }
    for (const auto& hookId : hooksJson.value("replace", json::array()))
    {
        Hook hook;
        hook.mod_id = hookId.value("mod_id", "");
        hook.script_path = hookId.value("script_path", "");
        hookContext->AddReplaceHook(hook);
    }
}

} // namespace ac

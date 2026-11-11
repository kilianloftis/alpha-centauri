#include "game/stages/TurnStageConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <stdexcept>
#include <unordered_set>

namespace ac
{

namespace
{

Hook_t ParseHook_(const ConfigReader& rReader)
{
    Hook_t hook;
    hook.modId = rReader.StringOr("modId", "");
    hook.scriptPath = rReader.StringOr("scriptPath", "");

    if (!hook.scriptPath.empty())
    {
        rReader.Fail("hook names scriptPath '" + hook.scriptPath
                     + "' but script loading is not available; remove the hook or bind a "
                       "callback in C++");
    }
    if (hook.modId.empty())
    {
        rReader.Fail("hook must provide modId; empty hooks are rejected");
    }
    return hook;
}

std::vector<Hook_t> ParseHookList_(const ConfigReader& rHooks, const char* pKey)
{
    std::vector<Hook_t> hooks;
    if (!rHooks.Has(pKey))
    {
        return hooks;
    }
    for (const ConfigReader& rHook : rHooks.Objects(pKey))
    {
        hooks.push_back(ParseHook_(rHook));
    }
    return hooks;
}

void ParseHooks_(const ConfigReader& rHooks, HookContext& rHookContext)
{
    for (const Hook_t& rHook : ParseHookList_(rHooks, "pre"))
    {
        rHookContext.AddPreHook(rHook);
    }
    for (const Hook_t& rHook : ParseHookList_(rHooks, "post"))
    {
        rHookContext.AddPostHook(rHook);
    }
    for (const Hook_t& rHook : ParseHookList_(rHooks, "replace"))
    {
        rHookContext.AddReplaceHook(rHook);
    }
}

TurnStageConfig_t ParseStage_(const ConfigReader& rReader)
{
    TurnStageConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.hookContext = HookContext(config.id);
    config.name = rReader.String("name");
    config.description = rReader.StringOr("description", "");
    config.bRepeatForEachFaction = rReader.Bool("repeatForEachFaction");
    if (rReader.Has("hooks"))
    {
        ParseHooks_(rReader.Object("hooks"), config.hookContext);
    }
    return config;
}

} // namespace

std::vector<TurnStageConfig_t> TurnStageConfigParser::ParseConfig(const std::string& configPath)
{
    std::vector<TurnStageConfig_t> stages =
        JsonConfigLoader::LoadFile<TurnStageConfig_t>(configPath, "turn stage", ParseStage_);

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

} // namespace ac

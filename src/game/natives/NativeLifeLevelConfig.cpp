#include "game/natives/NativeLifeLevelConfig.h"

#include "lib/config/ConfigSets.h"
#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <nlohmann/json.hpp>

#include <stdexcept>

namespace ac
{

namespace
{

NativeLifeLevel_t ParseLevel_(const nlohmann::json& rJson)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error("native life level entry must be a JSON object");
    }
    ConfigFields::RejectUnknownKeys(rJson, {"id", "name", "effects"}, "native life level");
    if (!rJson.contains("id") || !rJson.at("id").is_string()
        || rJson.at("id").get<std::string>().empty())
    {
        throw std::runtime_error("native life level missing non-empty string 'id'");
    }
    NativeLifeLevel_t level;
    level.id = rJson.at("id").get<std::string>();
    const std::string ctx = "native life level '" + level.id + "'";
    if (!rJson.contains("name") || !rJson.at("name").is_string())
    {
        throw std::runtime_error(ctx + ": 'name' must be a string");
    }
    level.name = rJson.at("name").get<std::string>();
    if (!rJson.contains("effects") || !rJson.at("effects").is_array())
    {
        throw std::runtime_error(ctx + ": 'effects' must be an array");
    }
    level.effects =
        EffectConfigParser::ParseEffects(rJson, EffectSourceKind_t::NativeLifeLevel, level.id);
    return level;
}

NativeLifeLevelConfig_t ParseNativeLifeLevels_(const nlohmann::json& rJson, const std::string& rCtx)
{
    ConfigFields::LevelSet_t<NativeLifeLevel_t> set =
        ConfigFields::ParseLevelSet<NativeLifeLevel_t>(rJson, rCtx, ParseLevel_);
    NativeLifeLevelConfig_t config;
    config.defaultId = std::move(set.defaultId);
    config.levels = std::move(set.levels);
    return config;
}

} // namespace

const NativeLifeLevel_t* NativeLifeLevelConfig_t::FindById(const std::string& rId) const
{
    for (const NativeLifeLevel_t& rLevel : levels)
    {
        if (rLevel.id == rId)
        {
            return &rLevel;
        }
    }
    return nullptr;
}

const NativeLifeLevel_t& NativeLifeLevelConfig_t::RequireForSession(
    const std::string& rLevelId) const
{
    const std::string& rWanted = rLevelId.empty() ? defaultId : rLevelId;
    if (const NativeLifeLevel_t* pLevel = FindById(rWanted))
    {
        return *pLevel;
    }
    throw std::runtime_error("Unknown native life level id '" + rWanted + "'");
}

NativeLifeLevelConfig_t NativeLifeLevelConfigParser::ParseConfig(const std::string& rConfigPath)
{
    return JsonConfigLoader::LoadObjectFile<NativeLifeLevelConfig_t>(
        rConfigPath, "native life levels", [&rConfigPath](const nlohmann::json& rJson) {
            return ParseNativeLifeLevels_(rJson,
                                          "Native life level config '" + rConfigPath + "'");
        });
}

} // namespace ac

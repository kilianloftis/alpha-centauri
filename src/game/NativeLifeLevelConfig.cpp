#include "game/NativeLifeLevelConfig.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace ac
{

namespace
{

void RejectUnknownKeys_(const nlohmann::json& rJson, const std::vector<std::string>& rKnown,
                        const std::string& rContext)
{
    for (const auto& [rKey, rUnused] : rJson.items())
    {
        if (std::find(rKnown.begin(), rKnown.end(), rKey) == rKnown.end())
        {
            throw std::runtime_error(rContext + ": unknown key '" + rKey + "'");
        }
    }
}

NativeLifeLevel_t ParseLevel_(const nlohmann::json& rJson)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error("native life level entry must be a JSON object");
    }
    RejectUnknownKeys_(rJson, {"id", "name", "effects"}, "native life level");
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
            const std::string ctx = "Native life level config '" + rConfigPath + "'";
            RejectUnknownKeys_(rJson, {"default", "levels"}, ctx);
            if (!rJson.contains("default") || !rJson.at("default").is_string()
                || rJson.at("default").get<std::string>().empty())
            {
                throw std::runtime_error(ctx + ": 'default' must be a non-empty string");
            }
            if (!rJson.contains("levels") || !rJson.at("levels").is_array()
                || rJson.at("levels").empty())
            {
                throw std::runtime_error(ctx + ": 'levels' must be a non-empty array");
            }

            NativeLifeLevelConfig_t config;
            config.defaultId = rJson.at("default").get<std::string>();
            std::unordered_set<std::string> seenIds;
            for (const nlohmann::json& rLevelJson : rJson.at("levels"))
            {
                NativeLifeLevel_t level = ParseLevel_(rLevelJson);
                if (!seenIds.insert(level.id).second)
                {
                    throw std::runtime_error(ctx + ": duplicate level id '" + level.id + "'");
                }
                config.levels.push_back(std::move(level));
            }
            if (!config.FindById(config.defaultId))
            {
                throw std::runtime_error(ctx + ": default '" + config.defaultId
                                         + "' does not match any level id");
            }
            return config;
        });
}

} // namespace ac

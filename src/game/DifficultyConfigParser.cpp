#include "game/DifficultyConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/config/ConfigSets.h"
#include "lib/config/JsonConfigLoader.h"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

int ReadRequiredThreshold_(const nlohmann::json& rJson, const char* key)
{
    if (!rJson.contains(key) || !rJson.at(key).is_number_integer())
    {
        throw std::runtime_error("'" + std::string(key) + "' must be an integer");
    }
    const int value = rJson.at(key).get<int>();
    if (value < 0)
    {
        throw std::runtime_error("'" + std::string(key) + "' must be >= 0");
    }
    return value;
}

bool ReadBool_(const nlohmann::json& rJson, const char* key, bool defaultValue)
{
    const auto it = rJson.find(key);
    if (it == rJson.end())
    {
        return defaultValue;
    }
    if (!it->is_boolean())
    {
        throw std::runtime_error(std::string("'") + key + "' must be a boolean");
    }
    return it->get<bool>();
}

bool ReadRequiredBool_(const nlohmann::json& rJson, const char* key)
{
    if (!rJson.contains(key) || !rJson.at(key).is_boolean())
    {
        throw std::runtime_error("'" + std::string(key) + "' must be a boolean");
    }
    return rJson.at(key).get<bool>();
}

int ReadNonNegativeInt_(const nlohmann::json& rJson, const char* key, int defaultValue)
{
    const auto it = rJson.find(key);
    if (it == rJson.end())
    {
        return defaultValue;
    }
    if (!it->is_number_integer())
    {
        throw std::runtime_error(std::string("'") + key + "' must be an integer");
    }
    const int value = it->get<int>();
    if (value < 0)
    {
        throw std::runtime_error(std::string("'") + key + "' must be >= 0");
    }
    return value;
}

DifficultyRules_t ParseRules_(const nlohmann::json& rJson, const std::string& rLevelId)
{
    const std::string ctx = "difficulty level '" + rLevelId + "' rules";
    if (!rJson.is_object())
    {
        throw std::runtime_error(ctx + " must be a JSON object");
    }
    static const std::vector<std::string> known = {
        "random_events_after_turn",
        "research_disabled_turns",
        "ai_secret_projects_require_human_prereq",
        "colony_pod_preserves_size_1_base",
        "no_power_overloads",
        "no_incited_pact_treaty_scripts",
        "ai_auto_personality",
        "combat_handicap",
        "combat_handicap_natives_only",
        "player_atrocity_threshold",
        "ai_atrocity_threshold",
        "planet_fauna_diff",
        "planet_fauna_swarm_event",
    };
    ConfigFields::RejectUnknownKeys(rJson, known, ctx);

    DifficultyRules_t rules;
    try
    {
        rules.randomEventsAfterTurn =
            ReadNonNegativeInt_(rJson, "random_events_after_turn", rules.randomEventsAfterTurn);
        rules.researchDisabledTurns =
            ReadNonNegativeInt_(rJson, "research_disabled_turns", rules.researchDisabledTurns);
        rules.aiSecretProjectsRequireHumanPrereq =
            ReadBool_(rJson, "ai_secret_projects_require_human_prereq",
                      rules.aiSecretProjectsRequireHumanPrereq);
        rules.colonyPodPreservesSize1Base =
            ReadBool_(rJson, "colony_pod_preserves_size_1_base",
                      rules.colonyPodPreservesSize1Base);
        rules.noPowerOverloads = ReadBool_(rJson, "no_power_overloads", rules.noPowerOverloads);
        rules.noIncitedPactTreatyScripts =
            ReadBool_(rJson, "no_incited_pact_treaty_scripts",
                      rules.noIncitedPactTreatyScripts);
        rules.aiAutoPersonality =
            ReadBool_(rJson, "ai_auto_personality", rules.aiAutoPersonality);
        rules.combatHandicap = ReadBool_(rJson, "combat_handicap", rules.combatHandicap);
        rules.combatHandicapNativesOnly =
            ReadBool_(rJson, "combat_handicap_natives_only", rules.combatHandicapNativesOnly);
        rules.playerAtrocityThreshold = ReadRequiredThreshold_(rJson, "player_atrocity_threshold");
        rules.aiAtrocityThreshold = ReadRequiredThreshold_(rJson, "ai_atrocity_threshold");
        rules.planetFaunaDiff = ReadRequiredThreshold_(rJson, "planet_fauna_diff");
        if (rules.planetFaunaDiff > 5)
        {
            throw std::runtime_error("'planet_fauna_diff' must be 0..5");
        }
        rules.planetFaunaSwarmEvent = ReadRequiredBool_(rJson, "planet_fauna_swarm_event");
    }
    catch (const std::runtime_error& rErr)
    {
        throw std::runtime_error(ctx + ": " + rErr.what());
    }

    if (rules.combatHandicapNativesOnly && !rules.combatHandicap)
    {
        throw std::runtime_error(
            ctx + ": combat_handicap_natives_only requires combat_handicap true");
    }
    return rules;
}

std::vector<EffectConfig_t> ParseEffects_(const nlohmann::json& rLevelJson,
                                          const std::string& rSourceId)
{
    if (!rLevelJson.contains("effects"))
    {
        return {};
    }
    return EffectConfigParser::ParseEffects(rLevelJson, EffectSourceKind_t::Difficulty,
                                            rSourceId);
}

DifficultyLevel_t ParseLevel_(const nlohmann::json& rJson)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error("difficulty level entry must be a JSON object");
    }
    static const std::vector<std::string> known = {
        "id", "name", "rules", "effects",
    };
    ConfigFields::RejectUnknownKeys(rJson, known, "difficulty level");

    if (!rJson.contains("id") || !rJson.at("id").is_string()
        || rJson.at("id").get<std::string>().empty())
    {
        throw std::runtime_error("difficulty level missing non-empty string 'id'");
    }
    DifficultyLevel_t level;
    level.id = rJson.at("id").get<std::string>();
    level.name = ConfigFields::ParseName(rJson, level.id, "name");

    if (!rJson.contains("rules"))
    {
        throw std::runtime_error("difficulty level '" + level.id
                                 + "' requires 'player_atrocity_threshold'");
    }
    level.rules = ParseRules_(rJson.at("rules"), level.id);

    level.effects = ParseEffects_(rJson, level.id);
    return level;
}

DifficultyConfig_t ParseDifficulty_(const nlohmann::json& rJson, const std::string& rConfigPath)
{
    ConfigFields::LevelSet_t<DifficultyLevel_t> set = ConfigFields::ParseLevelSet<DifficultyLevel_t>(
        rJson, "Difficulty config '" + rConfigPath + "'", ParseLevel_);
    DifficultyConfig_t config;
    config.defaultId = std::move(set.defaultId);
    config.levels = std::move(set.levels);
    return config;
}

} // namespace

DifficultyConfig_t DifficultyConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<DifficultyConfig_t>(
        configPath, "difficulty", [&configPath](const nlohmann::json& rJson) {
            return ParseDifficulty_(rJson, configPath);
        });
}

} // namespace ac

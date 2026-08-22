#include "game/DifficultyConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

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
    };
    RejectUnknownKeys_(rJson, known, ctx);

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
    RejectUnknownKeys_(rJson, known, "difficulty level");

    if (!rJson.contains("id") || !rJson.at("id").is_string()
        || rJson.at("id").get<std::string>().empty())
    {
        throw std::runtime_error("difficulty level missing non-empty string 'id'");
    }
    DifficultyLevel_t level;
    level.id = rJson.at("id").get<std::string>();
    level.name = rJson.value("name", level.id);

    if (rJson.contains("rules"))
    {
        level.rules = ParseRules_(rJson.at("rules"), level.id);
    }

    level.effects = ParseEffects_(rJson, level.id);
    return level;
}

} // namespace

DifficultyConfig_t DifficultyConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<DifficultyConfig_t>(
        configPath, "difficulty", [&configPath](const nlohmann::json& rJson) {
            const auto fail = [&](const std::string& rMessage) {
                throw std::runtime_error("Difficulty config '" + configPath + "': " + rMessage);
            };

            static const std::vector<std::string> knownKeys = {"default", "levels"};
            RejectUnknownKeys_(rJson, knownKeys, "Difficulty config '" + configPath + "'");

            if (!rJson.contains("default") || !rJson.at("default").is_string()
                || rJson.at("default").get<std::string>().empty())
            {
                fail("'default' must be a non-empty string");
            }
            if (!rJson.contains("levels") || !rJson.at("levels").is_array()
                || rJson.at("levels").empty())
            {
                fail("'levels' must be a non-empty array");
            }

            DifficultyConfig_t config;
            config.defaultId = rJson.at("default").get<std::string>();

            std::unordered_set<std::string> seenIds;
            for (const nlohmann::json& rLevelJson : rJson.at("levels"))
            {
                DifficultyLevel_t level = ParseLevel_(rLevelJson);
                if (!seenIds.insert(level.id).second)
                {
                    fail("duplicate level id '" + level.id + "'");
                }
                config.levels.push_back(std::move(level));
            }

            if (!config.FindById(config.defaultId))
            {
                fail("default '" + config.defaultId + "' does not match any level id");
            }
            return config;
        });
}

} // namespace ac

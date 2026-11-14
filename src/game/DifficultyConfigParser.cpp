#include "game/DifficultyConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/config/ConfigSets.h"
#include "lib/config/JsonConfigLoader.h"

#include <string>
#include <vector>

namespace ac
{

namespace
{

// The optional switches keep DifficultyRules_t's defaults when absent.
void ParseOptionalRules_(const ConfigReader& rReader, DifficultyRules_t& rRules)
{
    if (rReader.Has("random_events_after_turn"))
    {
        rRules.randomEventsAfterTurn = rReader.IntAtLeast("random_events_after_turn", 0);
    }
    if (rReader.Has("research_disabled_turns"))
    {
        rRules.researchDisabledTurns = rReader.IntAtLeast("research_disabled_turns", 0);
    }
    rRules.aiSecretProjectsRequireHumanPrereq = rReader.BoolOr(
        "ai_secret_projects_require_human_prereq", rRules.aiSecretProjectsRequireHumanPrereq);
    rRules.colonyPodPreservesSize1Base =
        rReader.BoolOr("colony_pod_preserves_size_1_base", rRules.colonyPodPreservesSize1Base);
    rRules.noPowerOverloads = rReader.BoolOr("no_power_overloads", rRules.noPowerOverloads);
    rRules.noIncitedPactTreatyScripts =
        rReader.BoolOr("no_incited_pact_treaty_scripts", rRules.noIncitedPactTreatyScripts);
    rRules.aiAutoPersonality = rReader.BoolOr("ai_auto_personality", rRules.aiAutoPersonality);
}

DifficultyRules_t ParseRules_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({
        "random_events_after_turn",
        "research_disabled_turns",
        "ai_secret_projects_require_human_prereq",
        "colony_pod_preserves_size_1_base",
        "no_power_overloads",
        "no_incited_pact_treaty_scripts",
        "ai_auto_personality",
        "player_atrocity_threshold",
        "ai_atrocity_threshold",
        "planet_fauna_diff",
        "planet_fauna_swarm_event",
    });

    DifficultyRules_t rules;
    ParseOptionalRules_(rReader, rules);
    rules.playerAtrocityThreshold = rReader.IntAtLeast("player_atrocity_threshold", 0);
    rules.aiAtrocityThreshold = rReader.IntAtLeast("ai_atrocity_threshold", 0);
    rules.planetFaunaDiff = rReader.IntInRange("planet_fauna_diff", 0, 5);
    rules.planetFaunaSwarmEvent = rReader.Bool("planet_fauna_swarm_event");
    return rules;
}

std::vector<EffectConfig_t> ParseEffects_(const ConfigReader& rReader,
                                          const std::string& rSourceId)
{
    if (!rReader.Has("effects"))
    {
        return {};
    }
    return EffectConfigParser::ParseEffects(rReader.Json(), EffectSourceKind_t::Difficulty,
                                            rSourceId);
}

DifficultyLevel_t ParseLevel_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"id", "name", "rules", "effects"});

    DifficultyLevel_t level;
    level.id = rReader.NonEmptyString("id");
    level.name = rReader.String("name");
    level.rules = ParseRules_(rReader.Object("rules"));
    level.effects = ParseEffects_(rReader, level.id);
    return level;
}

DifficultyConfig_t ParseDifficulty_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"default", "levels"});
    ConfigSets::LevelSet_t<DifficultyLevel_t> set =
        ConfigSets::ParseLevelSet<DifficultyLevel_t>(rReader, ParseLevel_);
    DifficultyConfig_t config;
    config.defaultId = std::move(set.defaultId);
    config.levels = std::move(set.levels);
    return config;
}

} // namespace

DifficultyConfig_t DifficultyConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<DifficultyConfig_t>(configPath, "difficulty",
                                                                ParseDifficulty_);
}

} // namespace ac

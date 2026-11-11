#include "game/council/CouncilRulesConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <cstddef>
#include <string>
#include <variant>

namespace ac
{

namespace
{

// CouncilEffects::SetGovernorEffects only reads FactionGlobal entries, so anything else here
// would be silently inert. One-shot governor perks live in on_elected_effects.
void ValidateGovernorEffectHonored_(const EffectConfig_t& rEffect, std::size_t index,
                                    const ConfigReader& rReader)
{
    if (rEffect.scope == EffectScope_t::FactionGlobal)
    {
        return;
    }
    rReader.FailKey("governor_effects[" + std::to_string(index) + "]",
                    "is not honored by the council runtime (a continuous governor effect must "
                    "be FactionGlobal; a one-shot perk belongs in 'on_elected_effects')");
}

std::vector<EffectConfig_t> ParseGovernorEffects_(const ConfigReader& rReader)
{
    // ParseEffects expects a container with an "effects" array; wrap the field.
    nlohmann::json wrapper = nlohmann::json::object();
    wrapper["effects"] = rReader.Array("governor_effects");
    std::vector<EffectConfig_t> effects = EffectConfigParser::ParseEffects(
        wrapper, EffectSourceKind_t::CouncilRules, "council_governor");
    for (std::size_t index = 0; index < effects.size(); ++index)
    {
        ValidateGovernorEffectHonored_(effects[index], index, rReader);
    }
    return effects;
}

CouncilRulesConfig_t ParseCouncilRules_(const ConfigReader& rReader)
{
    CouncilRulesConfig_t config;
    if (rReader.Has("governor_propose_interval_years"))
    {
        config.governorProposeIntervalYears =
            rReader.IntAtLeast("governor_propose_interval_years", 1);
    }
    if (rReader.Has("member_propose_interval_years"))
    {
        config.memberProposeIntervalYears = rReader.IntAtLeast("member_propose_interval_years", 1);
    }
    if (rReader.Has("governor_effects"))
    {
        config.governorEffects = ParseGovernorEffects_(rReader);
    }
    config.onElectedEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_elected_effects", "council_governor");
    return config;
}

} // namespace

CouncilRulesConfig_t CouncilRulesConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<CouncilRulesConfig_t>(configPath, "council rules",
                                                                  ParseCouncilRules_);
}

} // namespace ac

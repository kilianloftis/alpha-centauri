#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <variant>

namespace ac
{

namespace
{

// pop_growth.json carries exactly the two population baselines. Anything else reaching this
// list would join the faction pool as an AllOwnerBases continuous effect from a file whose
// name says nothing about it, so the stat set is closed rather than open.
void ValidateGrowthEffects_(const std::vector<EffectConfig_t>& rEffects,
                            const std::string& rConfigPath)
{
    const auto fail = [&](const std::string& rMessage) {
        throw std::runtime_error("Growth config '" + rConfigPath + "': " + rMessage);
    };

    bool bHasStartingSize = false;
    bool bHasMaxBaseSize = false;
    for (const EffectConfig_t& rEffect : rEffects)
    {
        const auto* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (!pMod)
        {
            fail("only StatModifier effects are allowed");
            continue;
        }
        switch (pMod->stat)
        {
            case StatId_t::StartingSize: bHasStartingSize = true; break;
            case StatId_t::MaxBaseSize:  bHasMaxBaseSize = true;  break;
            default:
                fail("stat is not allowed here (use starting_size or max_base_size)");
        }
        if (pMod->op != ModifierOp_t::Add)
        {
            fail("population baselines must use op Add");
        }
        if (pMod->amount <= 0.0)
        {
            fail("population baselines must be > 0, got " + std::to_string(pMod->amount));
        }
    }
    // Absent, these resolve to the Additive seed 0: StartingSize 0 throws on the first base
    // founded and MaxBaseSize 0 silently stops every base from ever growing. Both belong at
    // load time, next to the scalar keys they replaced.
    if (!bHasStartingSize)
    {
        fail("'effects' must contain a starting_size StatModifier");
    }
    if (!bHasMaxBaseSize)
    {
        fail("'effects' must contain a max_base_size StatModifier");
    }
}

} // namespace

GrowthConfig_t GrowthConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<GrowthConfig_t>(
        configPath, "population growth", [&configPath](const nlohmann::json& rJson) {
            // Required, not defaulted: nutrients_per_pop of 0 makes the growth threshold
            // identically 0, so every base grows every turn. The struct defaults exist for
            // programmatic construction, not for papering over a typo'd key.
            const auto readPositive = [&](const char* key) {
                const auto fail = [&](const std::string& rMessage) {
                    throw std::runtime_error("Growth config '" + configPath + "': '" + key + "' "
                                             + rMessage);
                };
                if (!rJson.contains(key))
                {
                    fail("is required");
                }
                if (!rJson.at(key).is_number_integer())
                {
                    fail("must be an integer");
                }
                const int value = rJson.at(key).get<int>();
                if (value <= 0)
                {
                    fail("must be > 0, got " + std::to_string(value));
                }
                return value;
            };

            GrowthConfig_t config;
            config.nutrientsPerPop = readPositive("nutrients_per_pop");
            config.nutrientIntakePerCitizen = readPositive("nutrient_intake_per_citizen");
            if (!rJson.contains("effects") || !rJson.at("effects").is_array())
            {
                throw std::runtime_error("Growth config '" + configPath
                                         + "': 'effects' is required and must be an array");
            }
            // Assigned, not appended: the struct's programmatic defaults exist for tests, and
            // must not survive into a config that declared its own baselines.
            config.effects = EffectConfigParser::ParseEffects(rJson, EffectSourceKind_t::Growth,
                                                              "pop_growth");
            ValidateGrowthEffects_(config.effects, configPath);
            return config;
        });
}

} // namespace ac

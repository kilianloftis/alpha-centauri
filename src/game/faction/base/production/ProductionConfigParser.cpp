#include "game/faction/base/production/ProductionConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "game/ConstructableKind.h"
#include "lib/config/JsonConfigLoader.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace ac
{

ProductionConfig_t ProductionConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<ProductionConfig_t>(
        configPath, "production", [&configPath](const nlohmann::json& rJson) {
            const auto read = [&](const char* key, int minimum, int maximum = -1) {
                const auto fail = [&](const std::string& rMessage) {
                    throw std::runtime_error("Production config '" + configPath + "': '" + key
                                             + "' " + rMessage);
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
                if (value < minimum)
                {
                    fail("must be >= " + std::to_string(minimum) + ", got "
                         + std::to_string(value));
                }
                if (maximum >= 0 && value > maximum)
                {
                    fail("must be <= " + std::to_string(maximum) + ", got "
                         + std::to_string(value));
                }
                return value;
            };

            ProductionConfig_t config;
            config.retoolPenaltyThreshold = read("retool_penalty_threshold", 0);
            config.retoolPenaltyPercent = read("retool_penalty_percent", 0, 100);
            // No upper bound: unlike the retool penalty (a percentage *of* a stockpile, so
            // >100 is meaningless), a surcharge above 100% is a coherent thing for a mod to ask
            // for — a prototype that costs triple.
            config.prototypeSurchargePercent = read("prototype_surcharge_percent", 0);

            if (!rJson.contains("hurry") || !rJson.at("hurry").is_object())
            {
                throw std::runtime_error("Production config '" + configPath
                                         + "': 'hurry' object is required");
            }
            // An empty object is legal: it turns hurrying off entirely, the same way dropping
            // a single kind turns it off for that kind. Each remaining key is a constructable
            // kind; stockpile is rejected because those items never complete.
            for (const auto& [rKind, rValue] : rJson.at("hurry").items())
            {
                const auto failKind = [&](const std::string& rMessage) {
                    throw std::runtime_error("Production config '" + configPath + "': hurry."
                                             + rKind + " " + rMessage);
                };
                if (rKind.empty())
                {
                    failKind("kind must be a non-empty string");
                }
                ConstructableKind_t kindId = ConstructableKind_t::Building;
                try
                {
                    kindId = ParseConstructableKind(rKind);
                }
                catch (const std::runtime_error&)
                {
                    failKind("is not a known constructable kind");
                }
                if (kindId == ConstructableKind_t::Stockpile)
                {
                    failKind("cannot be hurried; omit it from hurry");
                }
                if (!rValue.is_object())
                {
                    failKind("must be an object with formula, mineral_threshold, and "
                             "below_threshold_multiplier");
                }
                for (const auto& [rKey, rUnused] : rValue.items())
                {
                    if (rKey != "formula" && rKey != "mineral_threshold"
                        && rKey != "below_threshold_multiplier")
                    {
                        failKind("unknown key '" + rKey + "'");
                    }
                }
                const auto readKindInt = [&](const char* key, int minimum) {
                    if (!rValue.contains(key) || !rValue.at(key).is_number_integer())
                    {
                        failKind(std::string(key) + " must be an integer");
                    }
                    const int value = rValue.at(key).get<int>();
                    if (value < minimum)
                    {
                        failKind(std::string(key) + " must be >= " + std::to_string(minimum)
                                 + ", got " + std::to_string(value));
                    }
                    return value;
                };
                if (!rValue.contains("formula") || !rValue.at("formula").is_string()
                    || rValue.at("formula").get<std::string>().empty())
                {
                    failKind("formula must be a non-empty string");
                }
                HurryKindConfig_t kind;
                kind.formula = rValue.at("formula").get<std::string>();
                kind.mineralThreshold = readKindInt("mineral_threshold", 0);
                kind.belowThresholdMultiplier = readKindInt("below_threshold_multiplier", 1);
                config.hurryKinds.emplace(kindId, std::move(kind));
            }

            config.effects = EffectConfigParser::ParseEffects(
                rJson, EffectSourceKind_t::Production, "production");
            return config;
        });
}

} // namespace ac

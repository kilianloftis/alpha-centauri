#include "game/faction/base/production/ProductionConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "game/ConstructableKind.h"
#include "lib/config/JsonConfigLoader.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

ProductionConfig_t ProductionConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<ProductionConfig_t>(
        configPath, "production", [&configPath](const nlohmann::json& rJson) {
            const auto fail = [&](const std::string& rMessage) {
                throw std::runtime_error("Production config '" + configPath + "': " + rMessage);
            };

            static const std::vector<std::string> knownKeys = {
                "retool_penalty_threshold", "retool_penalty_percent",
                "prototype_surcharge_percent", "kinds", "effects",
            };
            for (const auto& [rKey, rUnused] : rJson.items())
            {
                if (std::find(knownKeys.begin(), knownKeys.end(), rKey) == knownKeys.end())
                {
                    fail("unknown key '" + rKey + "'");
                }
            }

            const auto read = [&](const char* key, int minimum, int maximum = -1) {
                if (!rJson.contains(key))
                {
                    fail("'" + std::string(key) + "' is required");
                }
                if (!rJson.at(key).is_number_integer())
                {
                    fail("'" + std::string(key) + "' must be an integer");
                }
                const int value = rJson.at(key).get<int>();
                if (value < minimum)
                {
                    fail("'" + std::string(key) + "' must be >= " + std::to_string(minimum)
                         + ", got " + std::to_string(value));
                }
                if (maximum >= 0 && value > maximum)
                {
                    fail("'" + std::string(key) + "' must be <= " + std::to_string(maximum)
                         + ", got " + std::to_string(value));
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

            if (!rJson.contains("kinds") || !rJson.at("kinds").is_object())
            {
                fail("'kinds' object is required");
            }

            const auto parseHurry = [&](const std::string& rKindPath, const nlohmann::json& rValue)
                -> HurryKindConfig_t {
                const auto failHurry = [&](const std::string& rMessage) {
                    fail(rKindPath + ".hurry " + rMessage);
                };
                if (!rValue.is_object())
                {
                    failHurry("must be an object with formula, mineral_threshold, and "
                              "below_threshold_multiplier");
                }
                for (const auto& [rKey, rUnused] : rValue.items())
                {
                    if (rKey != "formula" && rKey != "mineral_threshold"
                        && rKey != "below_threshold_multiplier")
                    {
                        failHurry("unknown key '" + rKey + "'");
                    }
                }
                const auto readKindInt = [&](const char* key, int minimum) {
                    if (!rValue.contains(key) || !rValue.at(key).is_number_integer())
                    {
                        failHurry(std::string(key) + " must be an integer");
                    }
                    const int value = rValue.at(key).get<int>();
                    if (value < minimum)
                    {
                        failHurry(std::string(key) + " must be >= " + std::to_string(minimum)
                                  + ", got " + std::to_string(value));
                    }
                    return value;
                };
                if (!rValue.contains("formula") || !rValue.at("formula").is_string()
                    || rValue.at("formula").get<std::string>().empty())
                {
                    failHurry("formula must be a non-empty string");
                }
                HurryKindConfig_t kind;
                kind.formula = rValue.at("formula").get<std::string>();
                kind.mineralThreshold = readKindInt("mineral_threshold", 0);
                kind.belowThresholdMultiplier = readKindInt("below_threshold_multiplier", 1);
                return kind;
            };

            const auto parseScrap = [&](const std::string& rKindPath, ConstructableKind_t kindId,
                                        const nlohmann::json& rValue) -> ScrapKindConfig_t {
                const auto failScrap = [&](const std::string& rMessage) {
                    fail(rKindPath + ".scrap " + rMessage);
                };
                if (kindId == ConstructableKind_t::SecretProject)
                {
                    failScrap("cannot be configured; secret projects cannot be scrapped");
                }
                if (!rValue.is_object())
                {
                    failScrap("must be an object with formula and refund_type");
                }
                for (const auto& [rKey, rUnused] : rValue.items())
                {
                    if (rKey != "formula" && rKey != "refund_type")
                    {
                        failScrap("unknown key '" + rKey + "'");
                    }
                }
                if (!rValue.contains("formula") || !rValue.at("formula").is_string()
                    || rValue.at("formula").get<std::string>().empty())
                {
                    failScrap("formula must be a non-empty string");
                }
                if (!rValue.contains("refund_type") || !rValue.at("refund_type").is_string()
                    || rValue.at("refund_type").get<std::string>().empty())
                {
                    failScrap("refund_type must be a non-empty string");
                }
                ScrapKindConfig_t kind;
                kind.formula = rValue.at("formula").get<std::string>();
                try
                {
                    kind.refundType = ParseScrapRefundType(rValue.at("refund_type").get<std::string>());
                }
                catch (const std::runtime_error&)
                {
                    failScrap("refund_type is not a known scrap refund type");
                }
                return kind;
            };

            // An empty object is legal: it turns hurrying and scrap off entirely. Each remaining
            // key is a constructable kind; stockpile is rejected because those items never
            // complete.
            for (const auto& [rKind, rValue] : rJson.at("kinds").items())
            {
                const std::string kindPath = "kinds." + rKind;
                if (rKind.empty())
                {
                    fail("kinds: kind must be a non-empty string");
                }
                ConstructableKind_t kindId = ConstructableKind_t::Building;
                try
                {
                    kindId = ParseConstructableKind(rKind);
                }
                catch (const std::runtime_error&)
                {
                    fail(kindPath + " is not a known constructable kind");
                }
                if (kindId == ConstructableKind_t::Stockpile)
                {
                    fail(kindPath + " cannot be hurried or scrapped; omit it from kinds");
                }
                if (!rValue.is_object())
                {
                    fail(kindPath + " must be an object with hurry and/or scrap");
                }
                for (const auto& [rKey, rUnused] : rValue.items())
                {
                    if (rKey != "hurry" && rKey != "scrap")
                    {
                        fail(kindPath + " unknown key '" + rKey + "'");
                    }
                }
                ProductionKindConfig_t kindConfig;
                if (rValue.contains("hurry"))
                {
                    kindConfig.hurry = parseHurry(kindPath, rValue.at("hurry"));
                }
                if (rValue.contains("scrap"))
                {
                    kindConfig.scrap = parseScrap(kindPath, kindId, rValue.at("scrap"));
                }
                if (!kindConfig.hurry && !kindConfig.scrap)
                {
                    fail(kindPath + " must include hurry or scrap");
                }
                config.kinds.emplace(kindId, std::move(kindConfig));
            }

            config.effects = EffectConfigParser::ParseEffects(
                rJson, EffectSourceKind_t::Production, "production");
            return config;
        });
}

} // namespace ac

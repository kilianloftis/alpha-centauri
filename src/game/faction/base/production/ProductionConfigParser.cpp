#include "game/faction/base/production/ProductionConfigParser.h"
#include "game/effects/EffectConfigParser.h"
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
            config.effects = EffectConfigParser::ParseEffects(
                rJson, EffectSourceKind_t::Production, "production");
            return config;
        });
}

} // namespace ac

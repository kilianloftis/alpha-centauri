#include "game/faction/base/production/ProductionConfigParser.h"
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
            const auto read = [&](const char* key, int minimum) {
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
                return value;
            };

            ProductionConfig_t config;
            // A minerals-per-row of 0 would make every item free.
            config.mineralsPerRow = read("minerals_per_row", 1);
            config.retoolPenaltyThreshold = read("retool_penalty_threshold", 0);
            config.retoolPenaltyNumerator = read("retool_penalty_numerator", 0);
            config.retoolPenaltyDenominator = read("retool_penalty_denominator", 1);
            if (config.retoolPenaltyNumerator > config.retoolPenaltyDenominator)
            {
                throw std::runtime_error(
                    "Production config '" + configPath
                    + "': retool penalty numerator exceeds denominator; a switch cannot forfeit "
                      "more minerals than were spent");
            }
            return config;
        });
}

} // namespace ac

#include "game/faction/CommerceConfigParser.h"

#include "lib/config/JsonConfigLoader.h"

#include <cmath>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace ac
{

CommerceConfig_t CommerceConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<CommerceConfig_t>(
        configPath, "commerce", [&configPath](const nlohmann::json& rJson) {
            const auto fail = [&](const std::string& rMessage) {
                throw std::runtime_error("Commerce config '" + configPath + "': " + rMessage);
            };

            for (const auto& [rKey, rUnused] : rJson.items())
            {
                if (rKey != "pair_multiplier" && rKey != "treaty_multiplier")
                {
                    fail("unknown key '" + rKey + "'");
                }
            }

            const auto readPositive = [&](const char* key) {
                if (!rJson.contains(key))
                {
                    fail("'" + std::string(key) + "' is required");
                }
                if (!rJson.at(key).is_number())
                {
                    fail("'" + std::string(key) + "' must be a number");
                }
                const double value = rJson.at(key).get<double>();
                if (!std::isfinite(value) || value <= 0.0)
                {
                    fail("'" + std::string(key) + "' must be finite and > 0");
                }
                return value;
            };

            CommerceConfig_t config;
            config.pairMultiplier = readPositive("pair_multiplier");
            config.treatyMultiplier = readPositive("treaty_multiplier");
            return config;
        });
}

} // namespace ac

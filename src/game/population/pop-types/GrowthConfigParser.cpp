#include "game/population/pop-types/GrowthConfigParser.h"
#include "lib/config/JsonConfigLoader.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace ac
{

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
            config.maxBaseSize = readPositive("max_base_size");
            return config;
        });
}

} // namespace ac

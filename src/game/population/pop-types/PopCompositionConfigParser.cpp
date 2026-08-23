#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

PopCompositionConfig_t PopCompositionConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<PopCompositionConfig_t>(
        configPath, "pop composition", [&configPath](const nlohmann::json& rJson) {
            const auto fail = [&](const std::string& rMessage) {
                throw std::runtime_error("Pop composition config '" + configPath + "': "
                                         + rMessage);
            };

            static const std::vector<std::string> knownKeys = {
                "bureaucracy_limit_formula", "drone_formula", "drone_type",
                "talent_formula", "talent_type",
                "assimilation_drones", "assimilation_decay_turns",
            };
            for (const auto& [rKey, rUnused] : rJson.items())
            {
                if (std::find(knownKeys.begin(), knownKeys.end(), rKey) == knownKeys.end())
                {
                    fail("unknown key '" + rKey + "'");
                }
            }

            const auto requireNonEmptyString = [&](const char* key) {
                if (!rJson.contains(key) || !rJson.at(key).is_string())
                {
                    fail(std::string("'") + key + "' must be a non-empty string");
                }
                const std::string value = rJson.at(key).get<std::string>();
                if (value.empty())
                {
                    fail(std::string("'") + key + "' must be a non-empty string");
                }
                return value;
            };

            const auto requirePositiveInt = [&](const char* key) {
                if (!rJson.contains(key) || !rJson.at(key).is_number_integer())
                {
                    fail(std::string("'") + key + "' must be a positive integer");
                }
                const int value = rJson.at(key).get<int>();
                if (value <= 0)
                {
                    fail(std::string("'") + key + "' must be > 0, got "
                         + std::to_string(value));
                }
                return value;
            };

            PopCompositionConfig_t config;
            config.bureaucracyLimitFormula = requireNonEmptyString("bureaucracy_limit_formula");
            config.droneFormula = requireNonEmptyString("drone_formula");
            config.droneTypeId = requireNonEmptyString("drone_type");
            config.talentFormula = requireNonEmptyString("talent_formula");
            config.talentTypeId = requireNonEmptyString("talent_type");
            config.assimilationDrones = requirePositiveInt("assimilation_drones");
            config.assimilationDecayTurns = requirePositiveInt("assimilation_decay_turns");
            return config;
        });
}

} // namespace ac

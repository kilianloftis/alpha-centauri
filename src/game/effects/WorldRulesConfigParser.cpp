#include "game/effects/WorldRulesConfigParser.h"

#include "game/effects/EffectConfigParser.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

std::vector<EffectConfig_t> WorldRulesConfigParser::ParseConfig(const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open world rules '" + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    return EffectConfigParser::ParseEffects(
        json, EffectSourceKind_t::WorldRules, "world_rules");
}

} // namespace ac

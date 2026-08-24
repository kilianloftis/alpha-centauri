#include "game/effects/PoliceRulesConfigParser.h"

#include "game/effects/EffectConfigParser.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

std::vector<EffectConfig_t> PoliceRulesConfigParser::ParseConfig(const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open police rules '" + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    return EffectConfigParser::ParseEffects(
        json, EffectSourceKind_t::PoliceRules, "police_rules");
}

} // namespace ac

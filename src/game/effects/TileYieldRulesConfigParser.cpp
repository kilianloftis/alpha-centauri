#include "game/effects/TileYieldRulesConfigParser.h"

#include "game/effects/BonusEffectParser.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

std::vector<EffectConfig_t> TileYieldRulesConfigParser::ParseConfig(const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open tile yield rules '" + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    return BonusEffectParser::ParseEffects(json);
}

} // namespace ac

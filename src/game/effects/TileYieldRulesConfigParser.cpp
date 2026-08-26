#include "game/effects/TileYieldRulesConfigParser.h"

#include "game/effects/EffectConfigParser.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

TileYieldRulesConfig_t TileYieldRulesConfigParser::ParseConfig(const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open tile yield rules '" + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);

    TileYieldRulesConfig_t config;
    config.elevationEnergyStepMeters =
        static_cast<int>(EffectConfigParser::RequireNumber(json, "elevation_energy_step_meters"));
    if (config.elevationEnergyStepMeters <= 0)
    {
        throw std::runtime_error("tile_yield_rules 'elevation_energy_step_meters' must be > 0, got "
                                 + std::to_string(config.elevationEnergyStepMeters));
    }
    config.effects = EffectConfigParser::ParseEffects(
        json, EffectSourceKind_t::TileYieldRules, "tile_yield_rules");
    return config;
}

} // namespace ac

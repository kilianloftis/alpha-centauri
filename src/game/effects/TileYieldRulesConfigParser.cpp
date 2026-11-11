#include "game/effects/TileYieldRulesConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

namespace ac
{

TileYieldRulesConfig_t TileYieldRulesConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<TileYieldRulesConfig_t>(
        configPath, "tile yield rules", [](const ConfigReader& rReader) {
            TileYieldRulesConfig_t config;
            config.elevationEnergyStepMeters =
                rReader.IntAtLeast("elevation_energy_step_meters", 1);
            config.effects = EffectConfigParser::ParseEffects(
                rReader.Json(), EffectSourceKind_t::TileYieldRules, "tile_yield_rules");
            return config;
        });
}

} // namespace ac

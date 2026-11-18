#include "game/combat/CombatConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <string>

namespace ac
{

CombatConfig_t CombatConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<CombatConfig_t>(
        configPath, "combat", [](const ConfigReader& rReader) {
            rReader.RejectUnknownKeys({"boarding_home_base_min_pop", "effects"});
            CombatConfig_t config;
            config.boardingHomeBaseMinPop = rReader.IntAtLeast("boarding_home_base_min_pop", 0);
            config.effects = EffectConfigParser::ParseEffects(
                rReader.Json(), EffectSourceKind_t::WorldRules, "combat");
            return config;
        });
}

} // namespace ac

#include "game/combat/CombatConfigParser.h"

#include "lib/config/JsonConfigLoader.h"

#include <string>

namespace ac
{

CombatConfig_t CombatConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<CombatConfig_t>(
        configPath, "combat", [](const ConfigReader& rReader) {
            rReader.RejectUnknownKeys({"boarding_home_base_min_pop"});
            CombatConfig_t config;
            config.boardingHomeBaseMinPop = rReader.IntAtLeast("boarding_home_base_min_pop", 0);
            return config;
        });
}

} // namespace ac

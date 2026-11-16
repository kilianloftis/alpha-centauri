#include "game/units/HealingConfigParser.h"

#include "lib/config/JsonConfigLoader.h"

#include <string>

namespace ac
{

HealingConfig_t HealingConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<HealingConfig_t>(
        configPath, "healing", [](const ConfigReader& rReader) {
            rReader.RejectUnknownKeys({"base_heal_percent", "max_heal_percent",
                                       "block_heal_on_move_spend", "block_heal_on_attacked",
                                       "block_heal_on_combat", "block_heal_on_took_damage"});
            HealingConfig_t config;
            config.baseHealPercent = rReader.IntAtLeast("base_heal_percent", 0);
            config.maxHealPercent = rReader.IntAtLeast("max_heal_percent", 0);
            config.blockHealOnMoveSpend = rReader.Bool("block_heal_on_move_spend");
            config.blockHealOnAttacked = rReader.Bool("block_heal_on_attacked");
            config.blockHealOnCombat = rReader.Bool("block_heal_on_combat");
            config.blockHealOnTookDamage = rReader.Bool("block_heal_on_took_damage");
            return config;
        });
}

} // namespace ac

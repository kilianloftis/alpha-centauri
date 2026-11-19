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
                                       "block_heal_on_move_spend", "block_heal_on_was_attacker",
                                       "block_heal_on_was_defender", "block_heal_on_took_damage"});
            HealingConfig_t config;
            config.baseHealPercent = rReader.IntAtLeast("base_heal_percent", 0);
            config.maxHealPercent = rReader.IntAtLeast("max_heal_percent", 0);
            config.blockHealOnMoveSpend = rReader.Bool("block_heal_on_move_spend");
            config.blockHealOnWasAttacker = rReader.Bool("block_heal_on_was_attacker");
            config.blockHealOnWasDefender = rReader.Bool("block_heal_on_was_defender");
            config.blockHealOnTookDamage = rReader.Bool("block_heal_on_took_damage");
            return config;
        });
}

} // namespace ac

#include "game/population/PoliceRulesConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

namespace ac
{

std::vector<EffectConfig_t> PoliceRulesConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<std::vector<EffectConfig_t>>(
        configPath, "police rules", [](const ConfigReader& rReader) {
            return EffectConfigParser::ParseEffects(rReader.Json(),
                                                    EffectSourceKind_t::PoliceRules,
                                                    "police_rules");
        });
}

} // namespace ac

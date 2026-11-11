#include "game/effects/WorldRulesConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

namespace ac
{

std::vector<EffectConfig_t> WorldRulesConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<std::vector<EffectConfig_t>>(
        configPath, "world rules", [](const ConfigReader& rReader) {
            return EffectConfigParser::ParseEffects(rReader.Json(), EffectSourceKind_t::WorldRules,
                                                    "world_rules");
        });
}

} // namespace ac

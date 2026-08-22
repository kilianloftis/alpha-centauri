#include "game/units/BaseConquestConfigParser.h"

#include "game/effects/EffectConfigParser.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

BaseConquestConfig_t BaseConquestConfigParser::ParseConfig(const std::string& configPath) const
{
    std::ifstream file(configPath);
    if (!file)
    {
        throw std::runtime_error("BaseConquestConfigParser: cannot open " + configPath);
    }

    nlohmann::json json;
    file >> json;

    BaseConquestConfig_t config;
    config.effects = EffectConfigParser::ParseEffects(json, EffectSourceKind_t::BaseConquest,
                                                     "base_conquest");
    if (json.contains("escape_colony_pod"))
    {
        const nlohmann::json& rPod = json.at("escape_colony_pod");
        if (rPod.contains("components"))
        {
            config.escapeColonyPod.componentIds =
                rPod.at("components").get<std::vector<std::string>>();
        }
    }

    return config;
}

} // namespace ac

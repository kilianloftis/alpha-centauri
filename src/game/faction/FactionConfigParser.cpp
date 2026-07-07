#include "game/faction/FactionConfigParser.h"

#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "lib/effects/BonusEffectParser.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

std::vector<FactionConfig_t> FactionConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<FactionConfig_t>(
        configPath, "faction",
        [this](const nlohmann::json& rJson) { return ParseFactionConfig(rJson); });
}

FactionConfig_t FactionConfigParser::ParseFactionConfig(const nlohmann::json& factionJson)
{
    FactionConfig_t config;
    config.id = ConfigFields::ParseId(factionJson);
    config.name = ConfigFields::ParseName(factionJson, config.id);
    config.leader = factionJson.at("leader").get<std::string>();
    config.effects = BonusEffectParser::ParseEffects(factionJson, EffectSourceKind::Faction, config.id);
    return config;
}

} // namespace ac

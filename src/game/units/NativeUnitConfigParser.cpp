#include "game/units/NativeUnitConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"

#include <stdexcept>

namespace ac
{

std::vector<NativeUnitConfig_t> NativeUnitConfigParser::ParseConfig(const std::string& rConfigPath)
{
    return JsonConfigLoader::LoadPath<NativeUnitConfig_t>(
        rConfigPath, "native unit",
        [this](const nlohmann::json& rJson) { return ParseNativeUnitConfig(rJson); });
}

NativeUnitConfig_t NativeUnitConfigParser::ParseNativeUnitConfig(const nlohmann::json& rJson)
{
    NativeUnitConfig_t config;
    config.id = ConfigFields::ParseId(rJson);
    config.name = ConfigFields::ParseName(rJson, config.id);
    if (!rJson.contains("domain"))
    {
        throw std::runtime_error("Native unit '" + config.id + "' missing required 'domain'");
    }
    config.domain = EffectConfigParser::ParseUnitDomain(rJson.at("domain").get<std::string>());
    config.mineralCost = rJson.value("mineral_cost", 0);
    config.effects =
        EffectConfigParser::ParseEffects(rJson, EffectSourceKind_t::NativeUnit, config.id);
    return config;
}

} // namespace ac

#include "game/units/NativeUnitConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/ConfigFields.h"

#include <cmath>
#include <fstream>
#include <stdexcept>

namespace ac
{

namespace
{

int RequireWholeCount_(const nlohmann::json& rJson, const char* pKey)
{
    const double value = EffectConfigParser::RequireNumber(rJson, pKey);
    if (value != std::floor(value))
    {
        throw std::runtime_error(std::string("native_units '") + pKey + "' must be a whole number");
    }
    return static_cast<int>(value);
}

} // namespace

std::vector<NativeUnitConfig_t> NativeUnitConfigParser::ParseConfig(const std::string& rConfigPath)
{
    std::ifstream file(rConfigPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open native units '" + rConfigPath + "'");
    }
    const nlohmann::json json = nlohmann::json::parse(file);
    if (!json.is_object())
    {
        throw std::runtime_error("native_units root must be an object");
    }

    m_life.fungalBloomNativeLifeformsMin =
        RequireWholeCount_(json, "fungal_bloom_native_lifeforms_min");
    m_life.fungalBloomNativeLifeformsMax =
        RequireWholeCount_(json, "fungal_bloom_native_lifeforms_max");
    if (m_life.fungalBloomNativeLifeformsMin < 0)
    {
        throw std::runtime_error(
            "native_units 'fungal_bloom_native_lifeforms_min' must be >= 0, got "
            + std::to_string(m_life.fungalBloomNativeLifeformsMin));
    }
    if (m_life.fungalBloomNativeLifeformsMax < m_life.fungalBloomNativeLifeformsMin)
    {
        throw std::runtime_error(
            "native_units 'fungal_bloom_native_lifeforms_max' must be >= "
            "'fungal_bloom_native_lifeforms_min', got "
            + std::to_string(m_life.fungalBloomNativeLifeformsMax) + " < "
            + std::to_string(m_life.fungalBloomNativeLifeformsMin));
    }

    if (!json.contains("units") || !json.at("units").is_array())
    {
        throw std::runtime_error("native_units 'units' must be an array");
    }

    std::vector<NativeUnitConfig_t> units;
    for (const nlohmann::json& rUnit : json.at("units"))
    {
        units.push_back(ParseNativeUnitConfig(rUnit));
    }
    return units;
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
    config.onHoldEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rJson, "on_hold_effects", config.id);
    return config;
}

} // namespace ac

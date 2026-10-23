#include "game/atrocities/AtrocityConfigParser.h"

#include "lib/config/ConfigSets.h"
#include "lib/config/EnumNames.h"

#include <fstream>
#include <magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

int RequireInt_(const nlohmann::json& rJson, const std::string& rKey,
                const std::string& rContext)
{
    if (!rJson.contains(rKey) || !rJson.at(rKey).is_number_integer())
    {
        throw std::runtime_error(rContext + " requires an integer '" + rKey + "'");
    }
    return rJson.at(rKey).get<int>();
}

bool RequireBool_(const nlohmann::json& rJson, const std::string& rKey,
                  const std::string& rContext)
{
    if (!rJson.contains(rKey) || !rJson.at(rKey).is_boolean())
    {
        throw std::runtime_error(rContext + " requires a boolean '" + rKey + "'");
    }
    return rJson.at(rKey).get<bool>();
}

AtrocitySeverityConfig_t ParseSeverity_(const nlohmann::json& rJson, const std::string& rContext)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error(rContext + " must be a JSON object");
    }

    static const std::vector<std::string> known = {
        "eco_virtual_minerals",
        "universal_vendetta",
        "expel_from_council",
    };
    ConfigFields::RejectUnknownKeys(rJson, known, rContext);

    AtrocitySeverityConfig_t config;
    config.ecoVirtualMinerals = RequireInt_(rJson, "eco_virtual_minerals", rContext);
    config.bUniversalVendetta = RequireBool_(rJson, "universal_vendetta", rContext);
    config.bExpelFromCouncil = RequireBool_(rJson, "expel_from_council", rContext);
    return config;
}

} // namespace

AtrocitiesConfig_t AtrocityConfigParser::ParseConfig(const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open atrocities config '" + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    if (!json.is_object())
    {
        throw std::runtime_error("Atrocities config '" + configPath + "' must be a JSON object");
    }
    static const std::vector<std::string> knownKeys = {
        "severities",
        "sanction_years_per_atrocity",
    };
    ConfigFields::RejectUnknownKeys(json, knownKeys, "Atrocities config '" + configPath + "'");

    if (!json.contains("severities") || !json.at("severities").is_object())
    {
        throw std::runtime_error("Atrocities config '" + configPath
                                 + "' requires a 'severities' object");
    }

    AtrocitiesConfig_t config;
    config.severities =
        ConfigFields::ParseEnumKeyedObject<AtrocitySeverityId_t, AtrocitySeverityConfig_t>(
            json.at("severities"), "atrocity severity", "Atrocities config '" + configPath + "'",
            [](const nlohmann::json& rEntry, AtrocitySeverityId_t, const std::string& rName) {
                return ParseSeverity_(rEntry, "severities." + rName);
            });

    config.sanctionYearsPerAtrocity =
        RequireInt_(json, "sanction_years_per_atrocity", "atrocities config");
    if (config.sanctionYearsPerAtrocity < 0)
    {
        throw std::runtime_error("Atrocities sanction_years_per_atrocity must be >= 0");
    }

    return config;
}

} // namespace ac

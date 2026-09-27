#include "game/atrocities/AtrocityConfigParser.h"

#include "lib/config/EnumNames.h"

#include <algorithm>
#include <array>
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

void RejectUnknownKeys_(const nlohmann::json& rJson, const std::vector<std::string>& rKnown,
                        const std::string& rContext)
{
    for (const auto& [rKey, rUnused] : rJson.items())
    {
        if (std::find(rKnown.begin(), rKnown.end(), rKey) == rKnown.end())
        {
            throw std::runtime_error(rContext + ": unknown key '" + rKey + "'");
        }
    }
}

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
    RejectUnknownKeys_(rJson, known, rContext);

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
    RejectUnknownKeys_(json, knownKeys, "Atrocities config '" + configPath + "'");

    if (!json.contains("severities") || !json.at("severities").is_object())
    {
        throw std::runtime_error("Atrocities config '" + configPath
                                 + "' requires a 'severities' object");
    }

    AtrocitiesConfig_t config;
    std::array<bool, k_AtrocitySeverityCount> seen{};
    for (const auto& [rName, rEntry] : json.at("severities").items())
    {
        const AtrocitySeverityId_t id = EnumFromName<AtrocitySeverityId_t>(rName, "atrocity severity");
        const auto index = magic_enum::enum_index(id);
        if (!index || seen[*index])
        {
            throw std::runtime_error("Atrocities config '" + configPath
                                     + "' repeats severity '" + rName + "'");
        }
        seen[*index] = true;
        config.severities[*index] = ParseSeverity_(rEntry, "severities." + rName);
    }
    for (const AtrocitySeverityId_t id : magic_enum::enum_values<AtrocitySeverityId_t>())
    {
        const auto index = magic_enum::enum_index(id);
        if (!index || !seen[*index])
        {
            throw std::runtime_error("Atrocities config '" + configPath + "' is missing severity '"
                                     + std::string(magic_enum::enum_name(id)) + "'");
        }
    }

    config.sanctionYearsPerAtrocity =
        RequireInt_(json, "sanction_years_per_atrocity", "atrocities config");
    if (config.sanctionYearsPerAtrocity < 0)
    {
        throw std::runtime_error("Atrocities sanction_years_per_atrocity must be >= 0");
    }

    return config;
}

} // namespace ac

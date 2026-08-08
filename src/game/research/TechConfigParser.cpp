#include "game/research/TechConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

std::vector<TechConfig_t> TechConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<TechConfig_t>(
        configPath, "tech",
        [this](const nlohmann::json& rJson) { return ParseTechConfig_(rJson); });
}

TechConfig_t TechConfigParser::ParseTechConfig_(const nlohmann::json& techJson)
{
    TechConfig_t config;
    config.id = ConfigFields::ParseId(techJson);
    config.name = ConfigFields::ParseName(techJson, config.id);
    config.category = ParseGameCategoryField(techJson);
    // Required: base_cost feeds the cost formula, so an omitted key silently cheapens the tech.
    if (!techJson.contains("cost"))
    {
        throw std::runtime_error("Tech '" + config.id + "': missing required field 'cost'");
    }
    if (!techJson.at("cost").is_number_integer())
    {
        throw std::runtime_error("Tech '" + config.id + "': 'cost' must be an integer");
    }
    config.cost = techJson.at("cost").get<int>();
    if (config.cost < 0)
    {
        throw std::runtime_error("Tech '" + config.id + "': 'cost' must not be negative");
    }
    config.prerequisites = ConfigFields::ParseStringArray(techJson, "prerequisites");

    return config;
}

} // namespace ac

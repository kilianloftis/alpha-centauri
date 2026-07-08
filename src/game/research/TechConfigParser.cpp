#include "game/research/TechConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include <nlohmann/json.hpp>

namespace ac
{

TechConfigParser::TechConfigParser()
{
}

std::vector<TechConfig_t> TechConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<TechConfig_t>(
        configPath, "tech",
        [this](const nlohmann::json& rJson) { return ParseTechConfig(rJson); });
}

TechConfig_t TechConfigParser::ParseTechConfig(const nlohmann::json& techJson)
{
    TechConfig_t config;
    config.id = ConfigFields::ParseId(techJson);
    config.name = ConfigFields::ParseName(techJson, config.id);
    config.category = ConfigFields::ParseGameCategory(techJson);
    config.cost = techJson.value("cost", 0);
    config.prerequisites = ConfigFields::ParseStringArray(techJson, "prerequisites");

    return config;
}

} // namespace ac

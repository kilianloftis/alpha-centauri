#include "game/buildings/BaseSpriteSizesConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

constexpr const char* k_Context = "base_sprite_sizes";

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

BaseSpriteSizeStage_t ParseSizeStage_(const nlohmann::json& rEntry, size_t index, int previousMin)
{
    const std::string context = std::string(k_Context) + ": size_stages[" + std::to_string(index)
                                + "]";
    if (!rEntry.is_object())
    {
        throw std::runtime_error(context + " must be an object");
    }
    RejectUnknownKeys_(rEntry, {"min_population", "origin_y_ratio"}, context);
    if (!rEntry.contains("min_population") || !rEntry.at("min_population").is_number_integer())
    {
        throw std::runtime_error(context + ": 'min_population' must be an integer");
    }
    const int minPopulation = rEntry.at("min_population").get<int>();
    if (minPopulation < 1)
    {
        throw std::runtime_error(context + ": 'min_population' must be >= 1");
    }
    if (minPopulation <= previousMin)
    {
        throw std::runtime_error(context + ": 'min_population' must be strictly increasing");
    }
    BaseSpriteSizeStage_t stage;
    stage.minPopulation = minPopulation;
    if (rEntry.contains("origin_y_ratio"))
    {
        if (!rEntry.at("origin_y_ratio").is_number())
        {
            throw std::runtime_error(context + ": 'origin_y_ratio' must be a number");
        }
        stage.originYRatio = rEntry.at("origin_y_ratio").get<float>();
    }
    return stage;
}

std::vector<BaseSpriteSizeStage_t> ParseSizeStages_(const nlohmann::json& rJson)
{
    if (!rJson.contains("size_stages") || !rJson.at("size_stages").is_array())
    {
        throw std::runtime_error(std::string(k_Context) + ": 'size_stages' must be an array");
    }
    const nlohmann::json& rStages = rJson.at("size_stages");
    if (rStages.empty())
    {
        throw std::runtime_error(std::string(k_Context) + ": 'size_stages' must not be empty");
    }

    std::vector<BaseSpriteSizeStage_t> stages;
    stages.reserve(rStages.size());
    int previousMin = 0;
    for (size_t i = 0; i < rStages.size(); ++i)
    {
        BaseSpriteSizeStage_t stage = ParseSizeStage_(rStages.at(i), i, previousMin);
        previousMin = stage.minPopulation;
        stages.push_back(stage);
    }
    return stages;
}

std::vector<std::string> ParseStageBumpBuildings_(const nlohmann::json& rJson)
{
    if (!rJson.contains("stage_bump_buildings"))
    {
        return {};
    }
    const nlohmann::json& rBumps = rJson.at("stage_bump_buildings");
    if (!rBumps.is_array())
    {
        throw std::runtime_error(std::string(k_Context)
                                 + ": 'stage_bump_buildings' must be an array");
    }

    std::vector<std::string> ids;
    ids.reserve(rBumps.size());
    for (size_t i = 0; i < rBumps.size(); ++i)
    {
        const std::string context = std::string(k_Context) + ": stage_bump_buildings["
                                    + std::to_string(i) + "]";
        if (!rBumps.at(i).is_string())
        {
            throw std::runtime_error(context + " must be a string");
        }
        const std::string id = rBumps.at(i).get<std::string>();
        if (id.empty())
        {
            throw std::runtime_error(context + " must not be empty");
        }
        ids.push_back(id);
    }
    return ids;
}

BaseSpriteSizesConfig_t ParseBaseSpriteSizes_(const nlohmann::json& rJson)
{
    RejectUnknownKeys_(rJson, {"size_stages", "stage_bump_buildings"}, k_Context);
    BaseSpriteSizesConfig_t config;
    config.sizeStages = ParseSizeStages_(rJson);
    config.stageBumpBuildings = ParseStageBumpBuildings_(rJson);
    return config;
}

} // namespace

BaseSpriteSizesConfig_t BaseSpriteSizesConfigParser::ParseConfig(
    const std::string& rConfigPath) const
{
    return JsonConfigLoader::LoadObjectFile<BaseSpriteSizesConfig_t>(
        rConfigPath, "base sprite size",
        [](const nlohmann::json& rJson) { return ParseBaseSpriteSizes_(rJson); });
}

} // namespace ac

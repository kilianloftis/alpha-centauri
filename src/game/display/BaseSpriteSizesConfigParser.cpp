#include "game/display/BaseSpriteSizesConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <string>
#include <vector>

namespace ac
{

namespace
{

BaseSpriteSizeStage_t ParseSizeStage_(const ConfigReader& rReader, int previousMin)
{
    rReader.RejectUnknownKeys({"min_population", "origin_y_ratio"});
    BaseSpriteSizeStage_t stage;
    stage.minPopulation = rReader.IntAtLeast("min_population", 1);
    if (stage.minPopulation <= previousMin)
    {
        rReader.FailKey("min_population", "must be strictly increasing");
    }
    stage.originYRatio = rReader.FloatOr("origin_y_ratio", stage.originYRatio);
    return stage;
}

std::vector<BaseSpriteSizeStage_t> ParseSizeStages_(const ConfigReader& rReader)
{
    rReader.NonEmptyArray("size_stages");
    std::vector<BaseSpriteSizeStage_t> stages;
    int previousMin = 0;
    for (const ConfigReader& rStage : rReader.Objects("size_stages"))
    {
        BaseSpriteSizeStage_t stage = ParseSizeStage_(rStage, previousMin);
        previousMin = stage.minPopulation;
        stages.push_back(stage);
    }
    return stages;
}

std::vector<std::string> ParseStageBumpBuildings_(const ConfigReader& rReader)
{
    std::vector<std::string> ids = rReader.StringArray("stage_bump_buildings");
    for (const std::string& rId : ids)
    {
        if (rId.empty())
        {
            rReader.FailKey("stage_bump_buildings", "must not contain empty ids");
        }
    }
    return ids;
}

BaseSpriteSizesConfig_t ParseBaseSpriteSizes_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"size_stages", "stage_bump_buildings"});
    BaseSpriteSizesConfig_t config;
    config.sizeStages = ParseSizeStages_(rReader);
    config.stageBumpBuildings = ParseStageBumpBuildings_(rReader);
    return config;
}

} // namespace

BaseSpriteSizesConfig_t BaseSpriteSizesConfigParser::ParseConfig(
    const std::string& rConfigPath) const
{
    return JsonConfigLoader::LoadObjectFile<BaseSpriteSizesConfig_t>(
        rConfigPath, "base sprite sizes", ParseBaseSpriteSizes_);
}

} // namespace ac

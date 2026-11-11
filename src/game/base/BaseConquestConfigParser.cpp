#include "game/base/BaseConquestConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

namespace ac
{

namespace
{

NativeRaidConfig_t ParseNativeRaid_(const ConfigReader& rReader)
{
    NativeRaidConfig_t raid;
    raid.facilityChancePercent = rReader.IntInRange("facility_chance_percent", 0, 100);
    raid.popLoss = rReader.IntAtLeast("pop_loss", 0);
    return raid;
}

HeadquartersEscapeConfig_t ParseHeadquartersEscape_(const ConfigReader& rReader)
{
    HeadquartersEscapeConfig_t escape;
    escape.cost = rReader.IntAtLeast("cost", 0);
    escape.scoreSeed = rReader.IntAtLeast("score_seed", 0);
    escape.scoreScale = rReader.IntAtLeast("score_scale", 0);
    escape.otherRegionDistanceFactor = rReader.IntAtLeast("other_region_distance_factor", 0);
    // The escape score divides by region bases plus this times population, so it must not be 0.
    escape.populationFactor = rReader.IntAtLeast("population_factor", 1);
    return escape;
}

BaseConquestConfig_t ParseBaseConquest_(const ConfigReader& rReader)
{
    BaseConquestConfig_t config;
    config.effects = EffectConfigParser::ParseEffects(
        rReader.Json(), EffectSourceKind_t::BaseConquest, "base_conquest");
    if (rReader.Has("escape_colony_pod"))
    {
        config.escapeColonyPod.componentIds =
            rReader.Object("escape_colony_pod").StringArray("components");
    }
    config.nativeRaid = ParseNativeRaid_(rReader.Object("native_raid"));
    config.headquartersEscape = ParseHeadquartersEscape_(rReader.Object("headquarters_escape"));
    return config;
}

} // namespace

BaseConquestConfig_t BaseConquestConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<BaseConquestConfig_t>(configPath, "base conquest",
                                                                  ParseBaseConquest_);
}

} // namespace ac

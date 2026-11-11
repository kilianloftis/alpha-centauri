#include "game/atrocities/AtrocityConfigParser.h"

#include "lib/config/ConfigSets.h"
#include "lib/config/JsonConfigLoader.h"

#include <string>

namespace ac
{

namespace
{

AtrocitySeverityConfig_t ParseSeverity_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({
        "eco_virtual_minerals", "universal_vendetta", "expel_from_council",
        "friction",             "blemishes",          "blemishes_floor",
        "repeat_blemishes",
    });

    AtrocitySeverityConfig_t config;
    config.ecoVirtualMinerals = rReader.Int("eco_virtual_minerals");
    config.bUniversalVendetta = rReader.Bool("universal_vendetta");
    config.bExpelFromCouncil = rReader.Bool("expel_from_council");
    config.friction = rReader.IntAtLeast("friction", 0);
    config.blemishes = rReader.IntAtLeast("blemishes", 0);
    config.blemishesFloor = rReader.IntAtLeast("blemishes_floor", 0);
    config.repeatBlemishes = rReader.IntAtLeast("repeat_blemishes", 0);
    return config;
}

AtrocitiesConfig_t ParseAtrocities_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"severities", "sanction_years_per_atrocity"});

    AtrocitiesConfig_t config;
    config.severities =
        ConfigSets::ParseEnumKeyedObject<AtrocitySeverityId_t, AtrocitySeverityConfig_t>(
            rReader.Object("severities"), "atrocity severity",
            [](const ConfigReader& rEntry, AtrocitySeverityId_t) {
                return ParseSeverity_(rEntry);
            });
    config.sanctionYearsPerAtrocity = rReader.IntAtLeast("sanction_years_per_atrocity", 0);
    return config;
}

} // namespace

AtrocitiesConfig_t AtrocityConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<AtrocitiesConfig_t>(configPath, "atrocities",
                                                                ParseAtrocities_);
}

} // namespace ac

#include "game/economy/CommerceConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <string>

namespace ac
{

namespace
{

CommerceConfig_t ParseCommerce_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"pair_multiplier", "formula"});
    CommerceConfig_t config;
    config.formula = rReader.Formula(
        "formula",
        {"energy_ours", "energy_theirs", "pair_multiplier", "commerce_tech", "tech_denominator"});
    config.pairMultiplier = rReader.NumberAbove("pair_multiplier", 0.0);
    return config;
}

} // namespace

CommerceConfig_t CommerceConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<CommerceConfig_t>(configPath, "commerce",
                                                              ParseCommerce_);
}

} // namespace ac

#pragma once

#include "game/effects/BonusEffect.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

struct PopTypeConfig_t
{
    std::string id;
    std::string name;
    bool bIsDefault;
    bool bCanWorkTile;
    bool bPlayerAssignable;
    int riotContribution;
    int goldenAgeContribution;
    std::vector<std::string> obsoletes;
    std::string requiredTech;
    std::string fallbackPopTypeId;
    std::vector<EffectConfig_t> effects;
};

class PopTypeConfigParser
{
public:
    PopTypeConfigParser();
    ~PopTypeConfigParser() = default;

    std::vector<PopTypeConfig_t> ParseConfig(const std::string& configPath);

private:
    PopTypeConfig_t ParsePopTypeConfig(const nlohmann::json& popJson);
};

} // namespace ac

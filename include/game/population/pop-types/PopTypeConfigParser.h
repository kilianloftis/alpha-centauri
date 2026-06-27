#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

struct PopTileMultipliers_t
{
    float nutrients;
    float energy;
    float minerals;
};

struct PopGeneration_t
{
    int nutrients;  // Bonus added to tile nutrient production
    int energy;     // Bonus added to tile energy production
    int minerals;   // Bonus added to tile mineral production
    int econ;       // Direct economic output (specialists)
    int labs;       // Direct research output (specialists)
    int psych;      // Direct psych output (specialists)
};

struct PopTypeConfig_t
{
    std::string id;
    std::string name;
    bool bIsDefault;
    bool bCanWorkTile;
    bool bPlayerAssignable;
    PopTileMultipliers_t tileMultipliers;
    PopGeneration_t generation;
    int riotContribution;
    int goldenAgeContribution;
    std::vector<std::string> obsoletes;
    std::string requiredTech;
    std::string fallbackPopTypeId;
};

class PopTypeConfigtParser
{
public:
    PopTypeConfigtParser();
    ~PopTypeConfigtParser() = default;

    std::vector<PopTypeConfig_t> ParseConfig(const std::string& configPath);

private:
    PopTypeConfig_t ParsePopTypeConfig_t(const nlohmann::json& popJson);
};

} // namespace ac

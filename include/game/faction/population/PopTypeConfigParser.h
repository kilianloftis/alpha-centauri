#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

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
    int nutrients;
    int energy;
    int minerals;
    int econ;
    int labs;
    int psych;
};

struct PopTypeConfig
{
    std::string id;
    std::string name;
    bool bCanWorkTile;
    PopTileMultipliers_t tileMultipliers;
    PopGeneration_t generation;
    int riotContribution;
    int goldenAgeContribution;
    std::vector<std::string> obsoletes;
    std::string requiredTech;
};

class PopTypeConfigParser
{
public:
    PopTypeConfigParser();
    ~PopTypeConfigParser() = default;

    std::vector<PopTypeConfig> ParseConfig(const std::string& configPath);

private:
    PopTypeConfig ParsePopTypeConfig(const nlohmann::json& popJson);
};

} // namespace ac

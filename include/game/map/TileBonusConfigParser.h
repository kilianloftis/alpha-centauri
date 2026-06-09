#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ac
{

struct TileBonusConfig
{
    std::string id;
    std::string name;
    std::string description;
    int nutrients;
    int minerals;
    int energy;
    int frequency;
    std::string spritePath;
};

class TileBonusConfigParser
{
public:
    TileBonusConfigParser();
    ~TileBonusConfigParser() = default;

    std::vector<TileBonusConfig> ParseConfig(const std::string& configPath);

private:
    TileBonusConfig ParseTileBonusConfig(const nlohmann::json& bonusJson);
};

} // namespace ac

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ac
{

struct TileBonusConfig_t
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

class TileBonusConfig_tParser
{
public:
    TileBonusConfig_tParser();
    ~TileBonusConfig_tParser() = default;

    std::vector<TileBonusConfig_t> ParseConfig(const std::string& configPath);

private:
    TileBonusConfig_t ParseTileBonusConfig_t(const nlohmann::json& bonusJson);
};

} // namespace ac

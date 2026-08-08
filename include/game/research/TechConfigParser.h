#pragma once

#include "game/GameCategory.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

using TechId = std::string;

struct TechConfig_t
{
    std::string id;
    std::string name;
    GameCategory_t category;
    int cost;
    std::vector<std::string> prerequisites;
};

class TechConfigParser
{
public:
    TechConfigParser() = default;
    ~TechConfigParser() = default;

    std::vector<TechConfig_t> ParseConfig(const std::string& configPath);

private:
    TechConfig_t ParseTechConfig_(const nlohmann::json& techJson);
};

} // namespace ac

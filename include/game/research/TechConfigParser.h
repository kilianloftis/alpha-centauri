#pragma once

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
    std::string category;  // build, grow, discover, conquer, or modder-defined
    int cost;
    std::vector<std::string> prerequisites;
};

class TechConfigParser
{
public:
    TechConfigParser();
    ~TechConfigParser() = default;

    std::vector<TechConfig_t> ParseConfig(const std::string& configPath);

private:
    TechConfig_t ParseTechConfig(const nlohmann::json& techJson);
};

} // namespace ac

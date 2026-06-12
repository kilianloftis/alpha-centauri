#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

struct TechConfig
{
    std::string id;
    std::string name;
    int cost;
};

class TechConfigParser
{
public:
    TechConfigParser();
    ~TechConfigParser() = default;

    std::vector<TechConfig> ParseConfig(const std::string& configPath);

private:
    TechConfig ParseTechConfig(const nlohmann::json& techJson);
};

} // namespace ac

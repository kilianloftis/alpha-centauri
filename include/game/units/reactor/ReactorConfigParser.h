#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

struct ReactorConfig_t
{
    std::string id;
    std::string name;
    std::string requiredTech;
    int mineralCost;
    // TODO: Add reactor-specific stats (power output, energy capacity, etc.)
};

class ReactorConfigParser
{
public:
    ReactorConfigParser() = default;
    ~ReactorConfigParser() = default;

    std::vector<ReactorConfig_t> ParseConfig(const std::string& rConfigPath);

private:
    ReactorConfig_t ParseReactorConfig(const nlohmann::json& rReactorJson);
};

} // namespace ac

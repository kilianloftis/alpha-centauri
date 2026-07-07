#pragma once

#include "game/faction/FactionConfig.h"
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

namespace ac
{

class FactionConfigParser
{
public:
    FactionConfigParser() = default;
    ~FactionConfigParser() = default;

    std::vector<FactionConfig_t> ParseConfig(const std::string& configPath);

private:
    FactionConfig_t ParseFactionConfig(const nlohmann::json& factionJson);
};

} // namespace ac

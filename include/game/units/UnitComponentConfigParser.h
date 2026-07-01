#pragma once

#include "game/units/UnitComponentConfig.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

class UnitComponentConfigParser
{
public:
    UnitComponentConfigParser() = default;
    ~UnitComponentConfigParser() = default;

    std::vector<UnitComponentConfig_t> ParseConfig(const std::string& rConfigPath);

private:
    UnitComponentConfig_t ParseComponentConfig(const nlohmann::json& rComponentJson);
};

} // namespace ac

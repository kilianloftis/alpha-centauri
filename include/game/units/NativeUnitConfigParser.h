#pragma once

#include "game/units/NativeUnitConfig.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

class NativeUnitConfigParser
{
public:
    NativeUnitConfigParser() = default;
    ~NativeUnitConfigParser() = default;

    std::vector<NativeUnitConfig_t> ParseConfig(const std::string& rConfigPath);
    NativeUnitConfig_t ParseNativeUnitConfig(const nlohmann::json& rJson);
};

} // namespace ac

#pragma once

#include "game/natives/NativeUnitConfig.h"

#include "lib/config/ConfigReader.h"
#include <string>
#include <vector>

namespace ac
{

class NativeUnitConfigParser
{
public:
    static std::vector<NativeUnitConfig_t> ParseConfig(const std::string& rConfigPath);
    static NativeUnitConfig_t ParseNativeUnitConfig(const ConfigReader& rReader);
};

} // namespace ac

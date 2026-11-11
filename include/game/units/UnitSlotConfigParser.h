#pragma once

#include "game/units/UnitSlotConfig.h"
#include <string>
#include <vector>

namespace ac
{

class UnitSlotConfigParser
{
public:
    static std::vector<UnitSlotConfig_t> ParseConfig(const std::string& rConfigPath);
};

} // namespace ac

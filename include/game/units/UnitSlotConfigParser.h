#pragma once

#include "game/units/UnitSlotConfig.h"
#include <string>
#include <vector>

namespace ac
{

class UnitSlotConfigParser
{
public:
    std::vector<UnitSlotConfig_t> ParseConfig(const std::string& rConfigPath) const;
};

} // namespace ac

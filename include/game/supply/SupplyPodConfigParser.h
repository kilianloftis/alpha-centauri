#pragma once

#include "game/supply/SupplyPodConfig.h"

#include <string>

namespace ac
{

class SupplyPodConfigParser
{
public:
    static SupplyPodConfig_t ParseConfig(const std::string& rConfigPath);
};

} // namespace ac

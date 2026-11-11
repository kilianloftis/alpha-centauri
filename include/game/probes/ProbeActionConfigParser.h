#pragma once

#include "game/probes/ProbeActionConfig.h"
#include <string>

namespace ac
{

class ProbeActionConfigParser
{
public:
    static ProbeActionsConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac

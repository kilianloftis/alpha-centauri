#pragma once

#include "game/probes/ProbeActionConfig.h"
#include <string>

namespace ac
{

class ProbeActionConfigParser
{
public:
    ProbeActionsConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac

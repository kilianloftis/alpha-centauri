#pragma once

#include "game/council/CouncilProposalConfig.h"
#include <string>
#include <vector>

namespace ac
{

class CouncilProposalConfigParser
{
public:
    std::vector<CouncilProposalConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac

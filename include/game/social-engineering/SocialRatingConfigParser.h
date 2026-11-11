#pragma once

#include "game/social-engineering/SocialRatingConfig.h"
#include <string>
#include <vector>

namespace ac
{

class SocialRatingConfigParser
{
public:
    static std::vector<SocialRatingConfig_t> ParseConfig(const std::string& rConfigPath);
};

} // namespace ac

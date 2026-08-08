#pragma once

#include "game/social-engineering/SocialRatingConfig.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

class SocialRatingConfigParser
{
public:
    SocialRatingConfigParser() = default;
    ~SocialRatingConfigParser() = default;

    std::vector<SocialRatingConfig_t> ParseConfig(const std::string& rConfigPath);

private:
    SocialRatingConfig_t ParseRatingConfig_(const nlohmann::json& rRatingJson);
};

} // namespace ac

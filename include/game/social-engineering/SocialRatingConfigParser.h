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
    SocialRatingConfigParser();
    ~SocialRatingConfigParser() = default;

    std::vector<SocialRatingConfig> ParseConfig(const std::string& rConfigPath);

private:
    SocialRatingConfig ParseRatingConfig(const nlohmann::json& rRatingJson);
};

} // namespace ac

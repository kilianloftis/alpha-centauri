#pragma once

#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class SocialRatingRegistry : public Registry<SocialRatingConfig, SocialRatingConfigParser>
{
};

} // namespace ac

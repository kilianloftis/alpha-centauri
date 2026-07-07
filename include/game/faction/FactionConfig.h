#pragma once

#include "lib/effects/BonusEffect.h"
#include <string>
#include <vector>

namespace ac
{

struct FactionConfig_t
{
    std::string id;
    std::string name;
    std::string leader;
    std::vector<EffectConfig_t> effects;
};

} // namespace ac

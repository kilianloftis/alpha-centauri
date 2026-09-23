#pragma once

#include "game/effects/EffectConfig.h"
#include "game/units/UnitDomain.h"

#include <string>
#include <vector>

namespace ac
{

struct NativeUnitConfig_t
{
    std::string id;
    std::string name;
    UnitDomain_t domain = UnitDomain_t::Land;
    int mineralCost = 0;
    std::vector<EffectConfig_t> effects;
};

} // namespace ac

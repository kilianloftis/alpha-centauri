#pragma once

#include "lib/effects/BonusEffect.h"

#include <string>
#include <vector>

namespace ac
{

struct UnitComponentConfig_t
{
    std::string id;
    std::string name;
    std::string type; // e.g. "chassis", "weapon" — matches component_type in unit_slot_config.json
    std::string requiredTech;
    int mineralCost = 0;
    std::vector<EffectConfig_t> effects;
};

} // namespace ac

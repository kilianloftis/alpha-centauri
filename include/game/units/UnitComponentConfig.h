#pragma once

#include "game/effects/BonusEffect.h"

#include <optional>
#include <string>
#include <vector>

namespace ac
{

// Movement / ZOC domain. Required on chassis components; land = neither sea nor air.
enum class UnitDomain_t
{
    Land,
    Sea,
    Air,
};

struct UnitComponentConfig_t
{
    std::string id;
    std::string name;
    std::string type; // e.g. "chassis", "weapon" — matches component_type in unit_slot_config.json
    std::string requiredTech;
    int mineralCost = 0;
    // Required when type == "chassis"; must be unset for other component types.
    std::optional<UnitDomain_t> domain;
    std::vector<EffectConfig_t> effects;
};

} // namespace ac

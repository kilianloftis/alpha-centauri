#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"
#include "game/units/UnitDomain.h"

#include <string>
#include <vector>

namespace ac
{

// Session-wide native-life rules from config/native_units.json, beside the unit list.
struct NativeLifeConfig_t
{
    // Inclusive uniform range for how many native lifeforms a fungal bloom spawns.
    int fungalBloomNativeLifeformsMin = 0;
    int fungalBloomNativeLifeformsMax = 0;
};

struct NativeUnitConfig_t
{
    std::string id;
    std::string name;
    UnitDomain_t domain = UnitDomain_t::Land;
    int mineralCost = 0;
    std::vector<EffectConfig_t> effects;
    // Considered when a unit of this design is ordered to Hold, and again when a building
    // is completed on its tile. A condition names the place (BaseHasBuilding, TargetTileHas).
    std::vector<TriggeredEffectConfig_t> onHoldEffects;
};

} // namespace ac

#pragma once

#include "game/effects/EffectConfig.h"
#include <vector>

namespace ac
{

// config/tile_yield_rules.json: `effects` joins every faction's pool, and the scalars feed
// per-tile amount sources.
struct TileYieldRulesConfig_t
{
    // Metres per solar altitude level after shore: energy is 1 + floor((elev − ocean) / step)
    // on land. Separate from map_rules.json's raise/lower level so a mod can move either.
    int elevationEnergyStepMeters = 0;
    std::vector<EffectConfig_t> effects;
};

} // namespace ac

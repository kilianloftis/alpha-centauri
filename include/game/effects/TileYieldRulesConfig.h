#pragma once

#include "game/effects/EffectConfig.h"
#include <vector>

namespace ac
{

// config/tile_yield_rules.json: `effects` joins every faction's pool, and the scalars feed
// per-tile amount sources.
struct TileYieldRulesConfig_t
{
    // Metres of elevation per point of solar energy: ceil(elevation / step), at least 0. Separate
    // from map_rules.json's level so a mod can move either.
    int elevationEnergyStepMeters = 0;
    std::vector<EffectConfig_t> effects;
};

} // namespace ac

#pragma once

#include "game/effects/EffectConfig.h"
#include <vector>

namespace ac
{

// config/tile_yield_rules.json — the world-level rules for what a tile yields.
// Two consumers: `effects` is merged into every faction's pool (FactionGlobal
// MaxClamp resource caps and friends), and the scalars below are read during per-tile
// resolution by amount sources that scale a modifier off terrain state.
struct TileYieldRulesConfig_t
{
    // Metres of elevation per point of solar-collector energy: a tile's contribution is
    // ceil(elevation / step), clamped at 0 so sea level and below yield nothing.
    // Deliberately not the same knob as a play-time elevation level
    // (config/map_rules.json): one is a yield rule and the other a terrain edit,
    // and a mod may want to move either alone.
    int elevationEnergyStepMeters = 0;
    std::vector<EffectConfig_t> effects;
};

} // namespace ac

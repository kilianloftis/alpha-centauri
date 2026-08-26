#pragma once

#include "game/effects/EffectConfig.h"
#include <vector>

namespace ac
{

// config/tile_yield_rules.json — the world-level rules for what a tile yields.
// Two consumers: `effects` is merged into every faction's pool (FactionGlobal
// TileResourceCap and friends), and the scalars below are read during per-tile
// resolution by amount sources that scale a modifier off terrain state.
struct TileYieldRulesConfig_t
{
    // Metres of elevation per point of solar-collector energy: a tile's contribution is
    // ceil(elevation / step), clamped at 0 so sea level and below yield nothing.
    // Deliberately not the same knob as the Former raise/lower step (TerraformRules.cpp's
    // k_ElevationStepMeters): they ship the same number, but one is a yield rule and the
    // other a terraform rule, and a mod may want to move either alone.
    int elevationEnergyStepMeters = 0;
    std::vector<EffectConfig_t> effects;
};

} // namespace ac

#pragma once

#include "game/effects/EffectConfig.h"

#include <vector>

namespace ac
{

class Unit;

// Coexistence overrides on rUnit whose scope is ThisUnit or FactionUnits and whose
// condition passes. Stock excludes stay in force for every pair these do not flip.
std::vector<CoexistenceOverrideEffect_t> ActiveCoexistenceOverrides(const Unit& rUnit);

} // namespace ac

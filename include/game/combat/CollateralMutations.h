#pragma once

#include "game/units/Unit.h"

#include <vector>

namespace ac
{

class Tile;
class UnitPositionIndex;

// Splash onto the units left on rTile once its defender is gone, as PlanStackCollateral plans it.
// Wild natives die outright; with NonCombatantsDestroyedWithoutCombatant, non-combatants die once
// no combatant is left. Every unit it kills goes through RemoveDefeatedUnit; returns the ids of
// those captured.
std::vector<UnitId_t> ApplyStackCollateral(UnitPositionIndex& rPositions, Unit& rAttacker,
                                           const Tile& rTile);

} // namespace ac

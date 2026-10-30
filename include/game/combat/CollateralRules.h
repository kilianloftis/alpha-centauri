#pragma once

#include <vector>

namespace ac
{

class Tile;
class Unit;

// What collateral does to one other occupant of the defender's tile.
struct CollateralHit_t
{
    Unit* pOccupant = nullptr;
    // A MaxClamp that leaves collateral_susceptibility at 0 shields the occupant.
    bool bImmune = false;
    // A wild native is destroyed outright.
    bool bDestroyed = false;
    int damage = 0;
};

// One entry per unit left on rTile, the tile a defender just died on, in tile order.
std::vector<CollateralHit_t> PlanStackCollateral(const Unit& rAttacker, const Tile& rTile);

// Read after collateral lands: while NonCombatantsDestroyedWithoutCombatant is in force, the
// non-combatants left with no combatant beside them. Immune occupants count for neither.
std::vector<Unit*> NonCombatantsLeftUnprotected(const Unit& rAttacker, const Tile& rTile,
                                                const std::vector<CollateralHit_t>& rPlan);

} // namespace ac

#pragma once

#include "game/combat/AttackRules.h"

namespace ac
{

class Unit;

// Marks the attack, spends AttackMoveFragments, clears the order, and expends a SingleUse unit.
// Returns true when that destroyed rAttacker.
bool SpendAttack(Unit& rAttacker, AttackKind_t kind);

// Destroys rUnit when it is SingleUse, after the action that spends it. Returns true when it did.
bool ExpendIfSingleUse(Unit& rUnit);

} // namespace ac

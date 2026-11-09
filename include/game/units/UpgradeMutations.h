#pragma once

namespace ac
{

class Unit;
class UnitDesign;

// Pays energy, rebinds the unit to rNew, adjusts High_Morale XP, records built components.
// Returns false when the upgrade is invalid, moves were spent, or the treasury cannot pay.
bool TryUpgradeUnit(Unit& rUnit, const UnitDesign& rNew);

} // namespace ac

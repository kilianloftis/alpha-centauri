#pragma once

#include "game/base/BaseTypes.h"

#include <vector>

namespace ac
{

class BaseManager;
class Unit;
struct EnergyAllocation_t;

struct EnergySplit_t
{
    int econ = 0;
    int labs = 0;
    int psych = 0;
};

// Energy left after inefficiency: distance to the headquarters against the Efficiency rating.
// The headquarters itself loses nothing.
int EnergyAfterInefficiency(const BaseManager& rBase, int energy);

// Labs and psych take their floored shares and econ the remainder, so the split conserves energy.
EnergySplit_t SplitEnergy(const EnergyAllocation_t& rAllocation, int energy);

// Inefficiency, the faction's split, then the base's Econ / Labs / Psych modifiers.
EnergySplit_t AllocateBaseEnergy(const BaseManager& rBase, int energy);

// Worker pops' tiles, supply crawlers homed here, and the base tile.
TileResources_t WorkedYield(const BaseManager& rBase);

int MineralsForProduction(const BaseManager& rBase);

// Native life on fungus is free.
int UnitSupportCost(const Unit& rUnit);

// Per home-unit mineral charge after free slots, parallel to GetUnits() order.
std::vector<int> MineralSupportCharges(const BaseManager& rBase);

// Home-unit mineral charge this turn if every current unit stays (free slots applied
// in GetUnits() order).
int MineralSupportCost(const BaseManager& rBase);

// The last charged home unit while the charges exceed the mineral bank; null once they fit.
Unit* UnitToDisbandForSupport(const BaseManager& rBase);

} // namespace ac

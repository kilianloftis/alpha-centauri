#pragma once

#include "game/units/UnitPlan.h"

namespace ac
{

class UnitDesign;

// make_proto's plan for a composed design: a weapon's plan, else by domain, with land roles
// from offense, defense, speed and ability overrides (docs/thinker/terranx-map.md, "Unit plan").
UnitPlan_t DeriveUnitPlan(const UnitDesign& rDesign);

} // namespace ac

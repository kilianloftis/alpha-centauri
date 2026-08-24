#pragma once

namespace ac
{

class BaseManager;

// Extra drones at this base from home units outside owner territory: sum each unit's
// away_from_home_drones weight (double), resolve through base-level modifiers on the same
// stat, then floor. Police SE supplies MaxClamp / MultiplyGeometric; combat baseline comes
// from police_rules (IsCombatUnit). Independent of garrison police suppression.
int ComputeAwayFromHomeDrones(const BaseManager& rBase);

} // namespace ac

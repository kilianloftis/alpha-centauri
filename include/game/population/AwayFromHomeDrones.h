#pragma once

namespace ac
{

class BaseManager;

// Drones from home units off owner territory: summed weights, base modifiers, then floor.
// Independent of garrison police.
int ComputeAwayFromHomeDrones(const BaseManager& rBase);

} // namespace ac

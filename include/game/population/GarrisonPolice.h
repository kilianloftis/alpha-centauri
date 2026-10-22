#pragma once

namespace ac
{

class BaseManager;

// Drones suppressed by police on the base tile, best effectiveness first, up to max_police.
int ComputeGarrisonPoliceSuppression(const BaseManager& rBase);

} // namespace ac

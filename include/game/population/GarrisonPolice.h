#pragma once

namespace ac
{

class BaseManager;

// Drones suppressed by same-faction non-embarked units on the base tile with
// police_effectiveness > 0, taking the best effectiveness values up to max_police slots.
// Independent of away-from-home drone extras (both are tuned by the Police SE axis).
int ComputeGarrisonPoliceSuppression(const BaseManager& rBase);

} // namespace ac

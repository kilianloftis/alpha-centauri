#pragma once

#include <vector>

namespace ac
{

class BaseManager;

// Per home-unit mineral charge after free slots, parallel to GetUnits() order.
std::vector<int> MineralSupportCharges(const BaseManager& rBase);

// Home-unit mineral charge this turn if every current unit stays (free slots applied
// in GetUnits() order). ApplyMineralSupportAtBase spends this against the bank, or
// disbands until it fits.
int MineralSupportCost(const BaseManager& rBase);

} // namespace ac

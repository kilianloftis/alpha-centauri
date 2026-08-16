#pragma once

namespace ac
{

class BaseManager;

// Home-unit mineral charge this turn if every current unit stays (free slots applied
// in GetUnits() order). ApplyMineralSupportAtBase spends this against the bank, or
// disbands until it fits.
int MineralSupportCost(const BaseManager& rBase);

// Per-base mineral support charge / disband against this turn's mineral bank.
// Implementation detail for BaseManager::ApplyMineralSupport (UnitSupport → Faction → base).
void ApplyMineralSupportAtBase(BaseManager& rBase);

} // namespace ac

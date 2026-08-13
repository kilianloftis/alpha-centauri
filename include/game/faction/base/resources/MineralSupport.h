#pragma once

namespace ac
{

class BaseManager;

// Per-base mineral support charge / disband against this turn's mineral bank.
// Implementation detail for BaseManager::ApplyMineralSupport (ResourceCollection → Faction → base).
void ApplyMineralSupportAtBase(BaseManager& rBase);

} // namespace ac

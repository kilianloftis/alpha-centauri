#pragma once

namespace ac
{

class BaseManager;
struct StockpileConfig_t;

// Consume this base's remaining mineral bank and credit the stockpile's outputs.
//
// Yield is resolved against the stockpile config's own effects only, not the base effect
// pool: those modifiers already apply to the banks being credited, so folding them in here
// would count them a second time. A mod that wants to boost conversion puts the modifier on
// the stockpile item.
//
// Implementation detail for BaseManager::ConvertSurplusMinerals (SurplusConversion → Faction
// → base), mirroring ApplyMineralSupportAtBase.
void ApplyStockpileConversionAtBase(BaseManager& rBase, const StockpileConfig_t& rStockpile);

} // namespace ac

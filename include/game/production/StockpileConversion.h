#pragma once

namespace ac
{

class BaseManager;
struct StockpileConfig_t;

// Credits the outputs of `minerals` already taken. Resolved against the stockpile's own
// effects only, since base modifiers already apply to the banks credited.
void ApplyStockpileConversionAtBase(BaseManager& rBase, const StockpileConfig_t& rStockpile,
                                    int minerals);

} // namespace ac

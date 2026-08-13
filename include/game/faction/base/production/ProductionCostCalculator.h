#pragma once

namespace ac
{

struct BaseEffects_t;

// Calculates the effective mineral cost for a production item.
// Formula: baseCost * CostMultiplier * (1 + surchargePercent/100), floored at 1.
// CostMultiplier effects (e.g. Industry social-rating levels) are resolved from rBaseEffects
// — same pattern as GrowthCalculator resolving GrowthRate.
// surchargePercent is the prototype mineral penalty (0 when the item is not a prototype).
class ProductionCostCalculator
{
public:
    ProductionCostCalculator() = delete;

    // Returns the effective mineral cost. Always at least 1.
    static int ComputeCost(int baseCost, const BaseEffects_t& rBaseEffects,
                           int surchargePercent = 0);
};

} // namespace ac

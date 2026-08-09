#pragma once

namespace ac
{

struct BaseEffects_t;

// Calculates the effective mineral cost for a production item.
// Formula: baseCost * mineralsPerRow * CostMultiplier, floored at 1.
// CostMultiplier effects (e.g. Industry social-rating levels) are resolved from rBaseEffects
// — same pattern as GrowthCalculator resolving GrowthRate.
class ProductionCostCalculator
{
public:
    ProductionCostCalculator() = delete;

    // Returns the effective mineral cost. Always at least 1.
    static int ComputeCost(int baseCost, int mineralsPerRow,
                           const BaseEffects_t& rBaseEffects);
};

} // namespace ac

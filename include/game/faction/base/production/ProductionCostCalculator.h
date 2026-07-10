#pragma once

namespace ac
{

struct BaseEffects_t;

// Calculates the effective mineral cost for a production item.
// Formula: baseCost * k_MineralsPerRow * CostMultiplier, floored at 1.
// CostMultiplier effects (e.g. Industry social-rating levels) are resolved from rBaseEffects
// — same pattern as GrowthCalculator resolving GrowthRate.
class ProductionCostCalculator
{
public:
    // SMAC minerals-per-row at Industry 0 (normal production rate).
    static constexpr int k_MineralsPerRow = 10;

    ProductionCostCalculator() = delete;

    // Returns the effective mineral cost. Always at least 1.
    static int ComputeCost(int baseCost, const BaseEffects_t& rBaseEffects);
};

} // namespace ac

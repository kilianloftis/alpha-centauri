#pragma once

namespace ac
{

struct BaseEffects_t;

// Calculates the effective mineral cost for a production item.
// Formula: baseCost * CostMultiplier * (1 + surchargePercent/100 * PrototypeSurchargeScale),
// floored at 1. CostMultiplier and PrototypeSurchargeScale are resolved from rBaseEffects
// (Industry AddPercent on the former; Skunkworks MultiplyGeometric 0 on the latter).
// surchargePercent is production.json's prototype mineral penalty (0 when not a prototype).
// TODO: the surcharge multiplies CostMultiplier rather than adding to it, and the result
// rounds half away from zero while the retool penalty next door documents floor rounding "so
// the remainder favours the player". Both are guesses; the real stacking and rounding rules
// are not recorded anywhere.
class ProductionCostCalculator
{
public:
    ProductionCostCalculator() = delete;

    // Returns the effective mineral cost. Always at least 1.
    static int ComputeCost(int baseCost, const BaseEffects_t& rBaseEffects, int surchargePercent);
};

} // namespace ac

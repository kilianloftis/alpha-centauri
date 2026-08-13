#pragma once

namespace ac
{

struct BaseEffects_t;

// Calculates the effective mineral cost for a production item.
// Formula: baseCost * CostMultiplier * (1 + surchargePercent/100), floored at 1.
// CostMultiplier effects (e.g. Industry social-rating levels) are resolved from rBaseEffects
// — same pattern as GrowthCalculator resolving GrowthRate.
// surchargePercent is the prototype mineral penalty (0 when the item is not a prototype).
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

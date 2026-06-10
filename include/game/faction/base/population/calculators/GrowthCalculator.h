#pragma once

namespace ac
{

struct GrowthInputs_t
{
    int baseSize;               // current population size
    int growthRateModifier;     // percentage modifier on required nutrients (e.g. -25 = 25% faster growth)
    int nutrientBank;           // current nutrients accumulated toward next growth
};

enum class GrowthResult
{
    None,       // No state change, nutrient bank updated
    Growth,     // Nutrient threshold reached
    Starvation  // Nutrient bank went negative
};

struct GrowthState_t
{
    int nutrientBank;           // nutrients accumulated toward next growth
    int nutrientsRequired;      // total nutrients needed to grow (recomputed each turn)
    int nutrientSurplus;        // nutrientsPerTurn minus per-turn upkeep (nutrients leftover each turn)
};

// Stateless helper class for population growth calculations.
// Computes nutrient requirements and determines growth/starvation state.
// The required nutrient threshold is:
//   TODO: confirm exact formula from game rules
//   Currently: baseSize * 10, modified by growthRateModifier as a percentage
class GrowthCalculator
{
public:
    GrowthCalculator() = default;
    ~GrowthCalculator() = default;

    // Advance one turn: compute new bank, check thresholds.
    // Returns GrowthResult indicating state change.
    // nutrientBank in inputs is the current bank, output bank is returned via outNewBank.
    static GrowthResult CalculateGrowh(const GrowthInputs_t& inputs, int& outNewBank);

    // Nutrients required to grow given the provided inputs (stateless calculation).
    static int ComputeNutrientsRequired(int baseSize, int growthRateModifier);
};

} // namespace ac

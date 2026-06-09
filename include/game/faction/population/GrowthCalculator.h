#pragma once

#include "lib/Signal.h"

namespace ac
{

struct GrowthInputs_t
{
    int baseSize;               // current population size
    int nutrientsPerTurn;       // net nutrients produced per turn (after consumption)
    int growthRateModifier;     // percentage modifier on required nutrients (e.g. -25 = 25% faster growth)
};

struct GrowthState_t
{
    int nutrientBank;           // nutrients accumulated toward next growth
    int nutrientsRequired;      // total nutrients needed to grow (recomputed each turn)
    int nutrientSurplus;        // nutrientsPerTurn minus per-turn upkeep (nutrients leftover each turn)
};

// Accumulates nutrient surplus each turn and fires growth/starvation signals when thresholds are crossed.
// Call Accumulate(inputs) once per turn during the Population stage.
// The required nutrient threshold is:
//   TODO: confirm exact formula from game rules
//   Currently: baseSize * 10, modified by growthRateModifier as a percentage
class GrowthCalculator
{
public:
    GrowthCalculator() = default;
    ~GrowthCalculator() = default;

    // Advance one turn: accumulate surplus, check thresholds.
    // Emits on_growth if the bank reaches nutrientsRequired.
    // Emits on_starvation if the bank falls below zero.
    // Resets the bank on growth or starvation.
    void Accumulate(const GrowthInputs_t& inputs);

    // Current accumulated nutrients toward next growth.
    int GetNutrientBank() const;

    // Nutrients required to grow given the provided inputs (does not mutate state).
    static int ComputeNutrientsRequired(int baseSize, int growthRateModifier);

    // Signals
    Signal<> on_growth;     // nutrient bank reached threshold; base may grow
    Signal<> on_starvation; // nutrient bank went negative; base may shrink

private:
    int m_nutrientBank = 0;
};

} // namespace ac

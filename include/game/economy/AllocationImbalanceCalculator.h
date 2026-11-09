#pragma once

namespace ac
{

struct AllocationImbalanceConfig_t;
class LuaRuntime;

struct AllocationImbalanceLoss_t
{
    int econLossPercent = 0;
    int labsLossPercent = 0;
    int psychLossPercent = 0;
};

struct AllocationImbalanceInputs_t
{
    int imbalanceFactor = 0;
    int econPercent = 0;
    int labsPercent = 0;
    int psychPercent = 0;
};

// Slider-imbalance percent losses from energy_allocation.lua.
class AllocationImbalanceCalculator
{
public:
    AllocationImbalanceCalculator(const AllocationImbalanceConfig_t& rConfig, LuaRuntime& rLua);

    // Evaluates econ/labs/psych loss formulas. Throws if a formula fails or yields a loss
    // outside 0..100.
    AllocationImbalanceLoss_t LossPercents(const AllocationImbalanceInputs_t& rInputs) const;

    // (value × (100 − lossPercent) + 50) / 100 — nearest percent.
    int ApplyLoss(int value, int lossPercent) const;

private:
    const AllocationImbalanceConfig_t& m_rConfig;
    // Not const: EvalInt mutates interpreter globals.
    LuaRuntime& m_rLua;
};

} // namespace ac

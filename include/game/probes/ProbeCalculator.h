#pragma once

namespace ac
{

struct ProbeCostConfig_t;
class LuaRuntime;

// The variables a base mind-control cost formula reads.
struct MindControlCostInputs_t
{
    // Sums of garrison_probe_weight and garrison_probe_multiplier over the base tile.
    int garrisonWeight = 0;
    int garrisonMultiplier = 0;
    int mindControlTotal = 0;
    int population = 0;
    int energy = 0;
    // HQ distance after probe_distance_scale.
    int distance = 0;
};

// The variables a unit subversion cost formula reads.
struct SubvertCostInputs_t
{
    int mineralCost = 0;
    int energy = 0;
    int distance = 0;
};

// Evaluates probe_actions.json's formulas. Gathering the inputs, the cost multipliers and every
// refusal are ProbeRules' job.
class ProbeCalculator
{
public:
    explicit ProbeCalculator(LuaRuntime& rLua);

    int MindControlCost(const ProbeCostConfig_t& rCost,
                        const MindControlCostInputs_t& rInputs) const;
    int SubvertCost(const ProbeCostConfig_t& rCost, const SubvertCostInputs_t& rInputs) const;

private:
    // Not const: EvalInt mutates interpreter globals.
    LuaRuntime& m_rLua;
};

} // namespace ac

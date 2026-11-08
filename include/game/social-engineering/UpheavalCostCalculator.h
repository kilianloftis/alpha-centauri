#pragma once

namespace ac
{

struct UpheavalCostConfig_t;
class LuaRuntime;

struct UpheavalCostInputs_t
{
    int changedCategories = 0;
};

// Evaluates social_policies.json upheaval.cost_formula. Difficulty and alien scale are applied
// by SocialEngineeringRules after this returns.
class UpheavalCostCalculator
{
public:
    UpheavalCostCalculator(const UpheavalCostConfig_t& rConfig, LuaRuntime& rLua);

    int Quote(const UpheavalCostInputs_t& rInputs) const;

private:
    const UpheavalCostConfig_t& m_rConfig;
    // Not const: EvalInt mutates interpreter globals.
    LuaRuntime& m_rLua;
};

} // namespace ac

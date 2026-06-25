#pragma once

namespace ac
{

struct GrowthConfig_t;
class LuaRuntime;

// Calculates the nutrient threshold required for a base to grow one population.
// The formula is provided via a GrowthConfig_t loaded from Lua (see pop_growth.lua).
// Variables available to the formula: base_size, growth_rating
class GrowthCalculator
{
public:
    GrowthCalculator(const GrowthConfig_t& rConfig, LuaRuntime& rLua);
    ~GrowthCalculator() = default;

    // Returns the nutrient threshold required to grow given base size and growth rating.
    int ComputeNutrientsRequired(int baseSize, int growthRating) const;

private:
    const GrowthConfig_t* m_pConfig;
    LuaRuntime* m_pLua;
};

} // namespace ac

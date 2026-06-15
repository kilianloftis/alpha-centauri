#pragma once

namespace ac
{

struct GrowthConfig;
class LuaRuntime;

// Calculates the nutrient threshold required for a base to grow one population.
// The formula is provided via a GrowthConfig loaded from Lua (see pop_growth.lua).
// Variables available to the formula: base_size, growth_rating
class GrowthCalculator
{
public:
    GrowthCalculator(const GrowthConfig& rConfig, LuaRuntime& rLua);
    ~GrowthCalculator() = default;

    // Returns the nutrient threshold required to grow given base size and growth rating.
    int ComputeNutrientsRequired(int baseSize, int growthRating) const;

private:
    const GrowthConfig* m_pConfig;
    LuaRuntime* m_pLua;
};

} // namespace ac

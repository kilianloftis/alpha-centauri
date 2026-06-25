#pragma once

#include "game/faction/base/production/ProductionCostConfig.h"

namespace ac
{

class LuaRuntime;

// Calculates the effective mineral cost for a production item.
// The formula is provided via a ProductionCostConfig_t loaded from Lua (see production_cost.lua).
// Variables available to the formula: base_cost, industry_rating
class ProductionCostCalculator
{
public:
    ProductionCostCalculator(const ProductionCostConfig_t& rConfig, LuaRuntime& rLua);
    ~ProductionCostCalculator() = default;

    // Returns the effective mineral cost given the item's base cost and the base's industry rating.
    int ComputeCost(int baseCost, int industryRating) const;

private:
    const ProductionCostConfig_t* m_pConfig;
    LuaRuntime* m_pLua;
};

} // namespace ac

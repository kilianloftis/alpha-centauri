#pragma once

#include "game/map/ImprovementConfigParser.h"

namespace ac
{

class LuaRuntime;

// Flat energy_cost, or elev_band / nearest_base_distance Lua on RaiseLand / LowerLand.
class FormerEnergyCostCalculator
{
public:
    explicit FormerEnergyCostCalculator(LuaRuntime& rLua);

    int Cost(const FormerEnergyCost_t& rCost, int elevBand, int nearestBaseDistance) const;

private:
    LuaRuntime& m_rLua;
};

} // namespace ac

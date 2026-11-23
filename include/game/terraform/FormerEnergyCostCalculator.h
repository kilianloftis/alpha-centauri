#pragma once

#include "game/map/ImprovementConfigParser.h"

namespace ac
{

class LuaRuntime;

// Inputs for the raise/lower Lua quote (docs/thinker/terranx-map.md `terraform_cost`).
struct FormerEnergyCostInputs_t
{
    // Shore-relative altitude levels: |elev| / reference_level_meters.
    int elevBand = 0;
    bool bFungus = false;
    bool bAtSea = false;
    // Resolved SeaRaiseLowerCostScale (SeedFor 1; Doc Air MaxClamp 0.5). Land quotes 1; Lua
    // multiplies by sea_cost_seed at sea.
    double seaCostScale = 1.0;
    // Own-base map_range clamped 1..100, or 1 when the faction has no base.
    int nearestBaseDistance = 1;
    // Nearest own / non-Pact enemy base; zeros when the enemy scale does not apply.
    int ownPop = 0;
    int enemyPop = 0;
    int ownRange = 0;
    int enemyRange = 0;
};

// Flat energy_cost, or the raise/lower Lua formula on RaiseLand / LowerLand.
class FormerEnergyCostCalculator
{
public:
    explicit FormerEnergyCostCalculator(LuaRuntime& rLua);

    int Cost(const FormerEnergyCost_t& rCost, const FormerEnergyCostInputs_t& rInputs) const;

private:
    LuaRuntime& m_rLua;
};

} // namespace ac

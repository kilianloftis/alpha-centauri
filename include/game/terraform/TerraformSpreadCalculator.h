#pragma once

#include "lib/Formula.h"

namespace ac
{

class LuaRuntime;

// Forest/kelp growth sample count and neighbor preference from configured Lua formulas.
class TerraformSpreadCalculator
{
public:
    // Copies the attempts formula so a moved GameDataContext cannot dangle it.
    TerraformSpreadCalculator(Formula_t attemptsFormula, LuaRuntime& rLua);

    // map_tiles / (turn/4 + 32) under shipping config. 0 when mapTileCount <= 0.
    int GrowthAttempts(int mapTileCount, int turnIndex) const;

    // moisture and rockiness are Moisture_t / Rockiness_t ordinals.
    int NeighborScore(const Formula_t& rScoreFormula, int moisture, int rockiness) const;

private:
    Formula_t m_attemptsFormula;
    LuaRuntime& m_rLua;
};

} // namespace ac

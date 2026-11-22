#include "game/terraform/TerraformSpreadCalculator.h"

#include "lib/LuaRuntime.h"

#include <algorithm>
#include <utility>

namespace ac
{

TerraformSpreadCalculator::TerraformSpreadCalculator(Formula_t attemptsFormula, LuaRuntime& rLua)
    : m_attemptsFormula(std::move(attemptsFormula))
    , m_rLua(rLua)
{
}

int TerraformSpreadCalculator::GrowthAttempts(int mapTileCount, int turnIndex) const
{
    if (mapTileCount <= 0)
    {
        return 0;
    }
    const int turn = std::max(0, turnIndex);
    const FormulaVars_t vars = {
        {"map_tiles", static_cast<double>(mapTileCount)},
        {"turn", static_cast<double>(turn)},
    };
    return m_rLua.EvalInt(m_attemptsFormula, vars);
}

int TerraformSpreadCalculator::NeighborScore(const Formula_t& rScoreFormula, int moisture,
                                             int rockiness) const
{
    const FormulaVars_t vars = {
        {"moisture", static_cast<double>(moisture)},
        {"rockiness", static_cast<double>(rockiness)},
    };
    return m_rLua.EvalInt(rScoreFormula, vars);
}

} // namespace ac

#include "game/natives/PlanetFaunaCalculator.h"

#include "game/natives/NativeLifeConfig.h"
#include "lib/LuaRuntime.h"

#include <algorithm>
#include <unordered_map>

namespace ac
{

PlanetFaunaCalculator::PlanetFaunaCalculator(const NativeLifeConfig_t& rConfig, LuaRuntime& rLua)
    : m_rConfig(rConfig)
    , m_rLua(rLua)
{
}

int PlanetFaunaCalculator::BoatPeriod(int faunaForms) const
{
    const std::unordered_map<std::string, double> vars = {
        {"fauna_forms", static_cast<double>(faunaForms)},
    };
    return m_rLua.EvalInt(m_rConfig.boatPeriodFormula, vars);
}

int PlanetFaunaCalculator::EarlyLimitThreshold(int faunaForms, int faunaDiff) const
{
    const std::unordered_map<std::string, double> vars = {
        {"fauna_forms", static_cast<double>(faunaForms)},
        {"fauna_diff", static_cast<double>(faunaDiff)},
    };
    return m_rLua.EvalInt(m_rConfig.earlyLimitThresholdFormula, vars);
}

int PlanetFaunaCalculator::BoatCargo(int turnNumber, int faunaForms, int faunaDiff,
                                     double cargoScale) const
{
    const std::unordered_map<std::string, double> vars = {
        {"turn", static_cast<double>(turnNumber)},
        {"fauna_forms", static_cast<double>(faunaForms)},
        {"fauna_diff", static_cast<double>(faunaDiff)},
        {"cargo_scale", cargoScale},
    };
    return m_rLua.EvalInt(m_rConfig.boatCargoFormula, vars);
}

int PlanetFaunaCalculator::AmbushAttempts(int faunaForms) const
{
    const std::unordered_map<std::string, double> vars = {
        {"fauna_forms", static_cast<double>(faunaForms)},
    };
    return m_rLua.EvalInt(m_rConfig.ambushAttemptsFormula, vars);
}

int PlanetFaunaCalculator::SwarmYearThreshold(int faunaForms) const
{
    const std::unordered_map<std::string, double> vars = {
        {"fauna_forms", static_cast<double>(faunaForms)},
    };
    return m_rLua.EvalInt(m_rConfig.swarmYearThresholdFormula, vars);
}

int PlanetFaunaCalculator::LifecycleFloor(int turnNumber) const
{
    const int turn = std::max(0, turnNumber);
    int floor = 0;
    for (size_t i = 0; i < m_rConfig.planetLifecycleTurnThresholds.size(); ++i)
    {
        const int threshold = m_rConfig.planetLifecycleTurnThresholds[i];
        if (threshold <= 0)
        {
            continue;
        }
        if (turn < threshold)
        {
            return floor;
        }
        floor = static_cast<int>(i) + 1;
    }
    return floor;
}

} // namespace ac

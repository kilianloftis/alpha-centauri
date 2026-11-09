#include "game/economy/AllocationImbalanceCalculator.h"

#include "game/economy/AllocationImbalanceConfig.h"
#include "lib/LuaRuntime.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ac
{

namespace
{

int EvalLossPercent_(LuaRuntime& rLua, const std::string& rFormula,
                     const std::unordered_map<std::string, double>& rVars,
                     const char* pName)
{
    const int loss = rLua.EvalInt(rFormula, rVars);
    if (loss < 0 || loss > 100)
    {
        throw std::runtime_error(std::string(pName) + " produced " + std::to_string(loss)
                                 + "; loss percent must be 0..100");
    }
    return loss;
}

} // namespace

AllocationImbalanceCalculator::AllocationImbalanceCalculator(
    const AllocationImbalanceConfig_t& rConfig, LuaRuntime& rLua)
    : m_rConfig(rConfig)
    , m_rLua(rLua)
{
}

AllocationImbalanceLoss_t AllocationImbalanceCalculator::LossPercents(
    const AllocationImbalanceInputs_t& rInputs) const
{
    const std::unordered_map<std::string, double> vars = {
        {"econ_percent", static_cast<double>(rInputs.econPercent)},
        {"labs_percent", static_cast<double>(rInputs.labsPercent)},
        {"psych_percent", static_cast<double>(rInputs.psychPercent)},
        {"imbalance_factor", static_cast<double>(rInputs.imbalanceFactor)},
    };

    AllocationImbalanceLoss_t loss;
    loss.econLossPercent =
        EvalLossPercent_(m_rLua, m_rConfig.econLossFormula, vars, "econ_loss_formula");
    loss.labsLossPercent =
        EvalLossPercent_(m_rLua, m_rConfig.labsLossFormula, vars, "labs_loss_formula");
    loss.psychLossPercent =
        EvalLossPercent_(m_rLua, m_rConfig.psychLossFormula, vars, "psych_loss_formula");
    return loss;
}

int AllocationImbalanceCalculator::ApplyLoss(int value, int lossPercent) const
{
    if (value <= 0 || lossPercent <= 0)
    {
        return value;
    }
    return (value * (100 - lossPercent) + 50) / 100;
}

} // namespace ac

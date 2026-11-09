#include "game/units/UpgradeCostCalculator.h"

#include "game/units/UpgradeCostConfig.h"
#include "lib/LuaRuntime.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ac
{

UpgradeCostCalculator::UpgradeCostCalculator(const UpgradeCostConfig_t& rConfig, LuaRuntime& rLua)
    : m_rConfig(rConfig)
    , m_rLua(rLua)
{
}

int UpgradeCostCalculator::UpgradeEnergyCost(const UpgradeCostInputs_t& rInputs) const
{
    const std::unordered_map<std::string, double> vars = {
        {"new_base_cost", static_cast<double>(rInputs.newBaseCost)},
        {"old_weapon_mineral_cost", static_cast<double>(rInputs.oldWeaponMineralCost)},
        {"new_weapon_mineral_cost", static_cast<double>(rInputs.newWeaponMineralCost)},
        {"old_armor_mineral_cost", static_cast<double>(rInputs.oldArmorMineralCost)},
        {"new_armor_mineral_cost", static_cast<double>(rInputs.newArmorMineralCost)},
        {"target_unprototyped", rInputs.bTargetUnprototyped ? 1.0 : 0.0},
    };

    const int energy = m_rLua.EvalInt(m_rConfig.energyCostFormula, vars);
    if (energy < 0)
    {
        throw std::runtime_error("Upgrade energy cost formula produced " + std::to_string(energy)
                                 + "; energy cost must be non-negative");
    }
    return energy;
}

} // namespace ac

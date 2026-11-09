#pragma once

namespace ac
{

struct UpgradeCostConfig_t;
class LuaRuntime;

// Plain mineral inputs for one old→new design upgrade energy quote.
struct UpgradeCostInputs_t
{
    int oldWeaponMineralCost = 0;
    int newWeaponMineralCost = 0;
    int oldArmorMineralCost = 0;
    int newArmorMineralCost = 0;
    // New design GetBaseCost() in minerals.
    int newBaseCost = 0;
    bool bTargetUnprototyped = false;
};

// Energy to upgrade one unit between two designs.
class UpgradeCostCalculator
{
public:
    UpgradeCostCalculator(const UpgradeCostConfig_t& rConfig, LuaRuntime& rLua);

    // Evaluates unit_upgrade.lua energy_cost_formula. Throws if the formula fails or yields a
    // negative energy cost. UpgradeCostScale is applied by Rules afterward.
    int UpgradeEnergyCost(const UpgradeCostInputs_t& rInputs) const;

private:
    const UpgradeCostConfig_t& m_rConfig;
    // Not const: EvalInt mutates interpreter globals.
    LuaRuntime& m_rLua;
};

} // namespace ac

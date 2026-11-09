#pragma once

#include <string>

namespace ac
{

// energy_cost_formula expression from unit_upgrade.lua (returns energy credits).
struct UpgradeCostConfig_t
{
    std::string energyCostFormula;
};

} // namespace ac

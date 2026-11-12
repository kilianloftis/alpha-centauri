#pragma once

#include "lib/Formula.h"

#include <string>

namespace ac
{

// energy_cost_formula expression from unit_upgrade.lua (returns energy credits).
struct UpgradeCostConfig_t
{
    Formula_t energyCostFormula;
};

} // namespace ac

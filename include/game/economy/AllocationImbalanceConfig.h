#pragma once

#include "lib/Formula.h"

#include <string>

namespace ac
{

// Loss expressions from energy_allocation.lua (formula owns divisor / scale / caps).
struct AllocationImbalanceConfig_t
{
    // Lua expressions from energy_allocation.lua (e.g. "labs_loss_formula()").
    Formula_t labsLossFormula;
    Formula_t econLossFormula;
    Formula_t psychLossFormula;
};

} // namespace ac

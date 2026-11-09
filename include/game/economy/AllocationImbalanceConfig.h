#pragma once

#include <string>

namespace ac
{

// Loss expressions from energy_allocation.lua (formula owns divisor / scale / caps).
struct AllocationImbalanceConfig_t
{
    // Lua expressions from energy_allocation.lua (e.g. "labs_loss_formula()").
    std::string labsLossFormula;
    std::string econLossFormula;
    std::string psychLossFormula;
};

} // namespace ac

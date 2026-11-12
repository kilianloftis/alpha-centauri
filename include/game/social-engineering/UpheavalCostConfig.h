#pragma once

#include "lib/Formula.h"

#include <string>

namespace ac
{

// social_policies.json "upheaval" block: the Lua expression for the category cube.
struct UpheavalCostConfig_t
{
    // Evaluated with `changed_categories`. Shipping: (changed_categories + 1) ^ 3.
    Formula_t costFormula;
};

} // namespace ac

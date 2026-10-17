#pragma once

#include <string>

namespace ac
{

// Tunables for planetary commerce income. Loaded from config/commerce.json.
// The pair income expression is a single Lua formula; CommerceRate scales the result in
// C++ afterward (like ScrapRefund), then CommerceEnergyBonus is added.
struct CommerceConfig_t
{
    // Exposed to formula as globals.
    double pairMultiplier{};

    // Shipping SMAC pipeline (ceil pair → tech ratio). CommerceRate (faction effects plus the
    // pair's diplomatic status effects) and the flat bonus are applied in C++ after eval.
    std::string formula;
};

} // namespace ac

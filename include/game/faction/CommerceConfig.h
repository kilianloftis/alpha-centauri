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
    double treatyMultiplier{};

    // Shipping SMAC pipeline (ceil pair → tech ratio → treaty_factor); rate and flat bonus
    // are applied in C++ after eval.
    std::string formula;
};

} // namespace ac

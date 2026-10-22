#pragma once

#include <string>

namespace ac
{

// config/commerce.json: one Lua formula per pair, scaled by CommerceRate, plus
// CommerceEnergyBonus.
struct CommerceConfig_t
{
    // Exposed to formula as globals.
    double pairMultiplier{};

    // Shipping SMAC pipeline (ceil pair → tech ratio). CommerceRate (faction effects plus the
    // pair's diplomatic status effects) and the flat bonus are applied in C++ after eval.
    std::string formula;
};

} // namespace ac

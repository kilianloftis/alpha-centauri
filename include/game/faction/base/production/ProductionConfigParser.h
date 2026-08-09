#pragma once

#include <string>

namespace ac
{

struct ProductionConfig_t
{
    // SMAC minerals-per-row at Industry 0 (normal production rate). Multiplies an item's
    // declared base cost.
    int mineralsPerRow = 10;

    // Retooling: switching production away from what the base started the turn on forfeits half
    // the minerals already spent, but only once more than this many have accumulated. Switching
    // *back* to the turn's original item is free; switching to a third item pays again.
    int retoolPenaltyThreshold = 10;
    // Numerator/denominator rather than a float, so the loss is exact and moddable: 1/2 is SMAC.
    int retoolPenaltyNumerator = 1;
    int retoolPenaltyDenominator = 2;
};

class ProductionConfigParser
{
public:
    ProductionConfigParser() = default;
    ~ProductionConfigParser() = default;

    // Load production.json. Throws if the file cannot be opened or parsed, if any value is
    // negative, or if the penalty denominator is zero.
    ProductionConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac

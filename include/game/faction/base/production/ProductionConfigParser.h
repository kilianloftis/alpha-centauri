#pragma once

#include <string>

namespace ac
{

struct ProductionConfig_t
{
    // Retooling: switching production away from what the base started the turn on forfeits a
    // percentage of the minerals already spent, but only once more than this many have
    // accumulated. Switching *back* to the turn's original item is free; switching to a third
    // item pays again.
    int retoolPenaltyThreshold = 10;
    // Percent of the stockpile forfeited on a retool (SMAC is 50). Integer math: stockpile *
    // percent / 100, rounded down so the remainder favours the player.
    int retoolPenaltyPercent = 50;
};

class ProductionConfigParser
{
public:
    ProductionConfigParser() = default;
    ~ProductionConfigParser() = default;

    // Load production.json. Throws if the file cannot be opened or parsed, if threshold or
    // percent is negative, or if percent exceeds 100.
    ProductionConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac

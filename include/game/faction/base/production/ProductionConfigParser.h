#pragma once

#include "game/effects/EffectConfig.h"

#include <string>
#include <vector>

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
    // Extra mineral cost for a prototype unit (first fielding of any component on the
    // design). Applied once even if several components are new. 50 means 50% more; no upper
    // bound, so a mod can make prototypes arbitrarily expensive.
    int prototypeSurchargePercent = 50;
    // Continuous effects merged into every faction pool (source id "production"). Prototype
    // starting XP is a FactionUnits StartingExperience StatModifier with unitFilter IsPrototype.
    std::vector<EffectConfig_t> effects;
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

#pragma once

#include "game/effects/EffectConfig.h"
#include "game/faction/base/production/ScrapConfig.h"
#include "game/ConstructableKind.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ac
{

struct HurryKindConfig_t
{
    // Lua expression returning a whole credit cost. Variable `minerals` is the billed count
    // (remaining, with below-threshold minerals counted extra). Also bound: remaining,
    // stockpile, mineral_threshold, below_threshold_multiplier.
    std::string formula;
    // A base whose stockpile has not reached this many minerals is billed extra for the ones
    // that would take it there. Per-kind so a facility and a unit need not share a band.
    // Deliberately independent of retoolPenaltyThreshold.
    // TODO: the SMAC rule this stands in for is unverified — confirm the real mineral_threshold.
    int mineralThreshold = 10;
    // How many times each of those below-threshold minerals is billed. 1 disables the extra.
    int belowThresholdMultiplier = 2;
};

// Hurry and scrap for one IConstructable kind. Either block may be absent — that mechanic
// is then off for the kind. A kind listed in production.json kinds must have at least one.
struct ProductionKindConfig_t
{
    std::optional<HurryKindConfig_t> hurry;
    std::optional<ScrapKindConfig_t> defaultScrap;
};

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
    // Per constructable kind: hurry and/or default_scrap. A kind with no entry cannot be
    // hurried or scrapped. An empty kinds object turns both mechanics off. Stockpile is
    // rejected (those items never complete). Secret-project scrap is rejected (destruction
    // tombstones them). Unit components and buildings may partially override default_scrap.
    std::unordered_map<ConstructableKind_t, ProductionKindConfig_t> kinds;
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
    // percent is negative, if retool percent exceeds 100, or if the kinds block is missing
    // or a kind/hurry/default_scrap entry is malformed. Formula strings are not evaluated
    // here: a broken one throws where it is used, as tech-cost and pop-composition formulas do.
    ProductionConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac

#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"
#include "game/production/ScrapConfig.h"
#include "game/production/ConstructableKind.h"
#include "lib/Formula.h"

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
    // Lua returning a whole credit cost from `minerals` (billed, with the below-threshold extra)
    // and remaining, stockpile, mineral_threshold, below_threshold_multiplier.
    Formula_t formula;
    // Below this many minerals, a base pays extra for the shortfall; per kind.
    // TODO: confirm SMAC's real mineral_threshold.
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
    // Switching away from the turn's original item forfeits a percentage once more than this
    // has accumulated. Switching back is free.
    int retoolPenaltyThreshold = 10;
    // Percent of the stockpile forfeited on a retool (SMAC is 50). Integer math: stockpile *
    // percent / 100, rounded down so the remainder favours the player.
    int retoolPenaltyPercent = 50;
    // Prototype surcharge, applied once however many components are new; no upper bound.
    int prototypeSurchargePercent = 50;
    // Per kind: hurry and default_scrap; a kind without an entry cannot use them. Stockpile and
    // secret-project scrap are rejected.
    std::unordered_map<ConstructableKind_t, ProductionKindConfig_t> kinds;
    // Continuous effects merged into every faction pool (source id "production").
    std::vector<EffectConfig_t> effects;
    // One-shot effects fired when any unit is produced at a base (prototype XP, …).
    std::vector<TriggeredEffectConfig_t> onUnitProducedEffects;
};

class ProductionConfigParser
{
public:
    // Throws on bad values or a malformed kinds block. Formulas throw where they are used.
    static ProductionConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac

#pragma once

#include "game/ConstructableKind.h"

namespace ac
{

struct ProductionConfig_t;
struct HurryKindConfig_t;
class LuaRuntime;

// What the queued item still needs, and which hurry formula prices it.
struct HurryInputs_t
{
    int remainingMinerals = 0;
    int mineralStockpile = 0;
    // IConstructable::GetConstructableKind(). A kind with no entry in production.json
    // cannot be hurried — that is how a mod switches the mechanic off for an item class.
    ConstructableKind_t kind = ConstructableKind_t::Stockpile;
};

// Full-finish quote. bAvailable false means the item's kind has no hurry entry: hurry is
// disabled for it, and the UI should offer no button rather than a price.
struct HurryQuote_t
{
    bool bAvailable = false;
    int remainingMinerals = 0;
    int billedMinerals = 0;
    int creditCost = 0;
};

// Credits actually charged and minerals granted for a partial (or full) spend.
struct HurrySpend_t
{
    int creditsSpent = 0;
    int mineralsAdded = 0;
};

// Energy-for-minerals math. Formulas live in production.json per hurry kind, so this owns
// no rules of its own beyond how a partial payment is priced.
//
// **Marginal pricing.** Buying k minerals costs what it takes off the finish price:
// cost(k) = Finish(remaining, stockpile) - Finish(remaining - k, stockpile + k). Any sequence
// of partial payments therefore telescopes to exactly the one-shot price, whatever shape the
// configured formula has. Pricing a partial payment as a straight fraction of the finish cost
// does not: both the below-threshold band and a formula that grows faster than its input make
// two half-payments cheaper than one whole one.
class HurryProductionCalculator
{
public:
    // rConfig and rLua outlive every base (GameDataContext owns both).
    HurryProductionCalculator(const ProductionConfig_t& rConfig, LuaRuntime& rLua);
    ~HurryProductionCalculator() = default;

    // remaining + the still-needed minerals that sit at or below mineralThreshold, each
    // extra copy of those counting (multiplier - 1) more times. multiplier is >= 1.
    static int BilledMinerals(int remaining, int stockpile, int mineralThreshold, int multiplier);

    // Cost to finish outright. Unknown kind yields an unavailable quote, not a throw; a
    // formula that fails or returns a non-positive cost for a positive remaining throws.
    HurryQuote_t Quote(const HurryInputs_t& rInputs) const;

    // Largest whole number of minerals requestedCredits buys, and what that costs. Credits
    // that do not cover another whole mineral are left unspent. Throws if requestedCredits is
    // not positive or the kind cannot be hurried — check Quote().bAvailable first.
    //
    // Assumes the configured formula does not get cheaper as more minerals are needed, which
    // is what lets this bisect instead of pricing every mineral.
    HurrySpend_t ApplyCredits(const HurryInputs_t& rInputs, int requestedCredits) const;

private:
    const HurryKindConfig_t* FindKind_(ConstructableKind_t kind) const;
    const HurryKindConfig_t& RequireKind_(ConstructableKind_t kind) const;
    // Finish price for `remaining` minerals with `stockpile` already banked. 0 when nothing
    // remains, without consulting Lua.
    int FinishCost_(const HurryKindConfig_t& rKind, int remaining, int stockpile) const;

    const ProductionConfig_t* m_pConfig;
    // Not const: EvalInt mutates interpreter globals, and Quote is a query.
    LuaRuntime* m_pLua;
};

} // namespace ac

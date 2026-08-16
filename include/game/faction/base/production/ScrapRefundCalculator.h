#pragma once

#include "game/ConstructableKind.h"
#include "game/faction/base/production/ProductionConfigParser.h"

namespace ac
{

class LuaRuntime;

// Outcome of pricing a player scrap. bAvailable false means the kind has no scrap entry.
struct ScrapQuote_t
{
    bool bAvailable = false;
    int amount = 0;
    ScrapRefundType_t refundType = ScrapRefundType_t::EnergyCredits;
};

// Player-scrap refunds. Combat, probes, raze, and orbital fire do not use this.
//
// Formula and refund_type live in production.json kinds.<kind>.scrap. Variable `minerals` is
// the item's listed mineral cost. A kind with no scrap entry cannot be scrapped.
class ScrapRefundCalculator
{
public:
    // rConfig and rLua outlive every base (GameDataContext owns both).
    ScrapRefundCalculator(const ProductionConfig_t& rConfig, LuaRuntime& rLua);
    ~ScrapRefundCalculator() = default;

    // Amount and payout type for this mineral cost and kind. Unavailable when the kind has
    // no scrap entry. Throws if mineralCost is negative, or if the formula returns a
    // negative amount.
    ScrapQuote_t Quote(int mineralCost, ConstructableKind_t kind) const;

private:
    const ScrapKindConfig_t* FindKind_(ConstructableKind_t kind) const;

    const ProductionConfig_t* m_pConfig;
    LuaRuntime* m_pLua;
};

} // namespace ac

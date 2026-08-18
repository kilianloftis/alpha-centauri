#pragma once

#include "game/ConstructableKind.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/base/production/ProductionConfigParser.h"

#include <vector>

namespace ac
{

class LuaRuntime;

// Outcome of pricing a player scrap. bAvailable false means the kind has no default_scrap.
struct ScrapQuote_t
{
    bool bAvailable = false;
    int amount = 0;
    StatId_t refundType = StatId_t::EnergyCredits;
};

// Player-scrap pricing. Combat, probes, raze, and orbital fire do not use this.
//
// Default formula, refund_type, and refund_ceiling_percent live in production.json
// kinds.<kind>.default_scrap. Variable `minerals` is listed mineral cost. A kind with no
// default_scrap cannot be scrapped. Unit components and buildings may override formula and
// refund_type, or set formula to null to deny scrap. ScrapRefund StatModifiers then scale
// the formula amount; the kind's ceiling clamps last, so an item override can change what a
// refund is paid in but never mint more than the kind allows.
//
// This prices only — where the refund lands is ScrapPayout's job.
class ScrapRefundCalculator
{
public:
    // rConfig and rLua outlive every base (GameDataContext owns both).
    ScrapRefundCalculator(const ProductionConfig_t& rConfig, LuaRuntime& rLua);
    ~ScrapRefundCalculator() = default;

    // Amount and payout type for this mineral cost and kind. Unavailable when the kind has
    // no default_scrap, or rOverride cleared the formula. Throws if mineralCost is negative,
    // or if the formula (after bonuses) returns a negative amount.
    ScrapQuote_t Quote(int mineralCost,
                       ConstructableKind_t kind,
                       const ScrapOverride_t& rOverride = {},
                       const std::vector<ActiveEffect_t>& rBonusEffects = {}) const;

private:
    const ScrapKindConfig_t* FindKind_(ConstructableKind_t kind) const;

    const ProductionConfig_t* m_pConfig;
    LuaRuntime* m_pLua;
};

} // namespace ac

#pragma once

#include "game/effects/EffectEnums.h"
#include "game/base/BaseTypes.h"

#include <optional>

namespace ac
{

struct BaseEffects_t;
struct ScrapQuote_t;

// A priced scrap together with the sink that will receive it. Quoting builds this, and
// crediting consumes it, so the number shown to the player is the number granted: a
// base-destined refund with no base to land in is zeroed here rather than at payout time,
// where the quote could no longer see it.
struct ScrapPayout_t
{
    int amount = 0;
    StatId_t refundType = StatId_t::EnergyCredits;
    // Base credited for a base-destined refund. Empty for energy credits (the faction
    // treasury takes those) and when no base can receive one — amount is then 0.
    std::optional<BaseId_t> destBaseId;
};

// Pair rQuote with destBaseId, zeroing the amount when the refund needs a base and none was
// supplied. Throws if rQuote is unavailable or its refundType is not a creditable payout.
ScrapPayout_t PlanScrapPayout(const ScrapQuote_t& rQuote, std::optional<BaseId_t> destBaseId);

// baseCost * CostMultiplier * (1 + surchargePercent/100 * PrototypeSurchargeScale), at least 1.
// surchargePercent is production.json's prototype mineral penalty (0 when not a prototype).
// TODO: the surcharge multiplies CostMultiplier rather than adding to it, and the result
// rounds half away from zero while the retool penalty next door documents floor rounding "so
// the remainder favours the player". Both are guesses; the real stacking and rounding rules
// are not recorded anywhere.
int ComputeProductionCost(int baseCost, const BaseEffects_t& rBaseEffects, int surchargePercent);

} // namespace ac

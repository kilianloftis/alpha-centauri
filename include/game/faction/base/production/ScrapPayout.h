#pragma once

#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseTypes.h"

#include <optional>

namespace ac
{

class Faction;
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

// Credit rPayout and return the amount granted. Energy credits go to rFaction's treasury,
// minerals to the destination base's production stockpile, energy through that base's
// inefficiency and the faction sliders, and the rest to that base's resource banks. Throws if
// destBaseId names a base rFaction does not hold.
int CreditScrapRefund(const ScrapPayout_t& rPayout, Faction& rFaction);

} // namespace ac

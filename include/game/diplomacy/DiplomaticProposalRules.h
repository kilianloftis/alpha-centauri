#pragma once

#include "game/diplomacy/TradeItem.h"

namespace ac
{


// Whether rProposal may be offered and accepted: two met factions, something asked, a status
// only one StepUp away, each side able to deliver and afford what it offers while keeping a
// base, and no demand for the recipient's headquarters.
bool IsValidProposal(const DiplomaticProposal_t& rProposal);

// Whether giver can hand rItem to receiver; IsValidProposal adds the whole-side checks.
bool CanDeliver(FactionId_t giver, FactionId_t receiver, const TradeItem_t& rItem);

// CanDeliver for an item receiver asks of giver, which is never giver's headquarters.
bool CanDemand(FactionId_t giver, FactionId_t receiver, const TradeItem_t& rItem);

} // namespace ac

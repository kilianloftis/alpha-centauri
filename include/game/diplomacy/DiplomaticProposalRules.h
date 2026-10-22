#pragma once

#include "game/diplomacy/TradeItem.h"

namespace ac
{


// Whether rProposal may be offered and accepted: two met factions, something asked, a status
// only one StepUp away, and each side able to deliver and afford what it offers.
bool IsValidProposal(const DiplomaticProposal_t& rProposal);

} // namespace ac

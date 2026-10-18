#pragma once

#include "game/production/ProductionRules.h"

namespace ac
{

class Faction;

// Credit rPayout and return the amount granted. Energy credits go to rFaction's treasury,
// minerals to the destination base's production stockpile, energy through that base's
// inefficiency and the faction sliders, and the rest to that base's resource banks. Throws if
// destBaseId names a base rFaction does not hold.
int CreditScrapRefund(const ScrapPayout_t& rPayout, Faction& rFaction);

} // namespace ac

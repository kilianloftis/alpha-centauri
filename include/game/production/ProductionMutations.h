#pragma once

#include "game/production/ProductionRules.h"

namespace ac
{

class Faction;

// Credits rPayout: energy credits to the treasury, minerals to the base's production, energy
// through inefficiency and the sliders. Throws if rFaction does not hold destBaseId.
int CreditScrapRefund(const ScrapPayout_t& rPayout, Faction& rFaction);

} // namespace ac

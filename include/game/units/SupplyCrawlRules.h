#pragma once

#include "game/effects/EffectEnums.h"

namespace ac
{

class Unit;

// Whether rUnit may harvest resource from its tile: it needs the SupplyCrawl flag and a home
// base. Throws when resource is not one of k_CrawlResources.
bool CanStartSupplyCrawl(const Unit& rUnit, StatId_t resource);

} // namespace ac

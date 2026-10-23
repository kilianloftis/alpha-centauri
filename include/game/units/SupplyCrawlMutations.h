#pragma once

#include "game/effects/EffectEnums.h"

namespace ac
{

class Unit;

// Claims rUnit's tile for its home base and sets a SupplyCrawl order. False when the unit may
// not crawl or another base already works the tile.
bool TryStartSupplyCrawl(Unit& rUnit, StatId_t resource);

} // namespace ac

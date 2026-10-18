#pragma once

#include "game/base/BaseTypes.h"

#include <vector>

namespace ac
{

class BaseManager;
class WorldMap;

// Owners by TileIndex: a land base claims contiguous land within radius 7, a sea base contiguous
// sea within 3, and a contested tile goes to the nearest claimant, then the lower BaseId.
std::vector<FactionId_t> ClaimTerritory(const WorldMap& rWorldMap,
                                        const std::vector<const BaseManager*>& rBases);

} // namespace ac

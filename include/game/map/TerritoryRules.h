#pragma once

#include "game/base/BaseTypes.h"

#include <vector>

namespace ac
{

class BaseManager;
class WorldMap;

// The owner of every tile, indexed by TileIndex. A land base claims a Euclidean disk of radius 7
// (dx^2+dy^2 <= 50) of contiguous land; a sea base claims radius 3 (dx^2+dy^2 <= 10) of
// contiguous sea. A contested tile goes to the nearest claiming base, then the lower BaseId.
std::vector<FactionId_t> ClaimTerritory(const WorldMap& rWorldMap,
                                        const std::vector<const BaseManager*>& rBases);

} // namespace ac

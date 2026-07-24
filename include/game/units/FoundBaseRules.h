#pragma once

#include "game/faction/base/BaseTypes.h"
#include <vector>

namespace ac
{

class BaseManager;
class GameState;
class Tile;
class WorldMap;
class TerritoryMap;

// Colony pods may not found within this Chebyshev distance of any existing base
// (distance 0, 1, or 2 are illegal; 3+ is allowed).
inline constexpr int k_MinBaseFoundingSeparation = 3;

// True when any live base is within Chebyshev distance < k_MinBaseFoundingSeparation.
bool IsTooCloseToAnyBase(const Tile& rTile, const WorldMap& rWorldMap,
                         const std::vector<const BaseManager*>& rBases);

// True when the tile belongs to a faction other than the founder (unowned / own OK).
bool IsInForeignTerritory(const Tile& rTile, FactionId_t founderFactionId,
                          const TerritoryMap& rTerritory);

// Territory + spacing checks only (does not require a FoundBase-capable unit).
bool CanFoundBaseAt(const Tile& rTile, FactionId_t founderFactionId, const WorldMap& rWorldMap,
                    const std::vector<const BaseManager*>& rBases);

bool CanFoundBaseAt(const Tile& rTile, FactionId_t founderFactionId, const GameState& rGameState);

} // namespace ac

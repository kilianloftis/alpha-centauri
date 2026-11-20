#pragma once

#include "game/base/BaseTypes.h"
#include <string>
#include <vector>

namespace ac
{

class BaseManager;
class Tile;
class Unit;
class TerritoryMap;

// Colony pods may not found within this Chebyshev distance of any existing base
// (distance 0, 1, or 2 are illegal; 3+ is allowed).
inline constexpr int k_MinBaseFoundingSeparation = 3;

// True when any live base is within Chebyshev distance < k_MinBaseFoundingSeparation.
bool IsTooCloseToAnyBase(const Tile& rTile, const std::vector<const BaseManager*>& rBases);

// True when the tile belongs to a faction other than the founder (unowned / own OK).
bool IsInForeignTerritory(const Tile& rTile, FactionId_t founderFactionId,
                          const TerritoryMap& rTerritory);

// Spacing, territory, and whether the Base occupant can go on the tile: terrain it excludes
// (Rocky, Fungus, a Monolith) refuses, improvements it excludes are displaced. Does not require
// a founding-capable unit.
bool CanFoundBaseAt(const Tile& rTile, FactionId_t founderFactionId,
                    const std::vector<const BaseManager*>& rBases);

// Ids of the improvements founding on rTile removes: the ones the Base occupant excludes.
std::vector<std::string> ImprovementsDestroyedByFounding(const Tile& rTile);

// A unit with on_found_effects founds where it stands: not embarked, water only for sea
// domain (land/air/orbital on land only), and CanFoundBaseAt.
bool CanFoundBase(const Unit& rUnit);

} // namespace ac

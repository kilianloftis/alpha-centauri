#pragma once

#include "game/base/BaseTypes.h"

#include <optional>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;
class Tile;
class UnitDesign;
class WorldMap;

// The base a granted unit is homed at, most specific first: the context base when it is the
// faction's own, the base nearest the context tile, the headquarters, then any base.
std::optional<BaseId_t> GrantAnchorBase(const Faction& rFaction, const BaseManager* pContextBase,
                                        const Tile* pContextTile);

// Tiles a unit of rDesign may be placed on, nearest first: the origin, then whole rings out
// to radius 2 until there are `needed`. A crowded map yields fewer.
std::vector<const Tile*> PlacementTiles(const Tile& rOrigin, const UnitDesign& rDesign,
                                        FactionId_t factionId, int needed,
                                        const WorldMap& rWorldMap);

} // namespace ac

#pragma once

#include "game/map/Tile.h"

#include <string>
#include <vector>

namespace ac
{

class BaseManager;
class Unit;

struct SurfaceFlipConsequences_t
{
    std::vector<std::string> removedImprovements;
    // True when a base on new water may not occupy water and must be razed.
    bool bRazeBase = false;
    // Units whose domain no longer matches the surface. They die unless a base on the tile is
    // still standing after the flip.
    std::vector<Unit*> strandedUnits;
};

// A flip removes every improvement whose domain is not the new surface (an omitted domain stays),
// except the Base marker of a base on the tile. A base that may occupy water, or whose tile became
// land, is untouched and shelters the units on it; any other base on new water is razed.
SurfaceFlipConsequences_t SurfaceFlipConsequences(const Tile& rTile, bool bNowWater,
                                                  const BaseManager* pBase);

} // namespace ac

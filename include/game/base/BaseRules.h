#pragma once

#include "game/map/MapUtils.h"

namespace ac
{

class Faction;
class Tile;
class WorkerAssignmentManager;

// Land always; water needs may_occupy_water. Callers pass the surface and capability under
// test (current tile, a surface-flip forecast, or post-destroy occupancy).
bool MayBaseOccupySurface(bool bWater, bool bMayOccupyWater);

// terranx vulnerable: tabletop distance from rTile to rFaction's headquarters, or map_rules.json
// missing_headquarters_distance when it has none.
int HeadquartersDistance(const Faction& rFaction, const Tile& rTile);

// The SMAC base work area: Euclidean radius 2 on the lattice (p^2 + q^2 <= 5), the base tile
// excluded (20 surrounding tiles).
template<typename WorldMapT, typename Fn>
void ForEachWorkableTile(const Tile& rBaseTile, WorldMapT& rWorldMap, Fn&& fn)
{
    static constexpr int k_WorkableRadius = 2;
    ForEachTileInEuclideanRadius(rBaseTile, rWorldMap, k_WorkableRadius, /*includeOrigin=*/false,
        [&](auto* pTile, int /*distSq*/) { fn(pTile); });
}

// rFaction's bases in region, counted from current ownership (config/deviations/README.md,
// "Region base counts are live").
int RegionBaseCount(const Faction& rFaction, int region);

// Work areas may overlap, but a tile is worked by at most one base.
bool MayWorkTile(const WorkerAssignmentManager& rAssignments, const Tile& rTile);

} // namespace ac

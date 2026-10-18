#pragma once

#include "game/map/MapUtils.h"

namespace ac
{

class Faction;
class Tile;
class WorkerAssignmentManager;

// Founding a base while the faction has no headquarters makes that base the headquarters.
bool GrantsHeadquarters(const Faction& rFaction);

// The SMAC base work area: Euclidean radius 2 on the lattice (p^2 + q^2 <= 5), the base tile
// excluded (20 surrounding tiles).
template<typename WorldMapT, typename Fn>
void ForEachWorkableTile(const Tile& rBaseTile, WorldMapT& rWorldMap, Fn&& fn)
{
    static constexpr int k_WorkableRadius = 2;
    ForEachTileInEuclideanRadius(rBaseTile, rWorldMap, k_WorkableRadius, /*includeOrigin=*/false,
        [&](auto* pTile, int /*distSq*/) { fn(pTile); });
}

// Work areas may overlap, but a tile is worked by at most one base.
bool MayWorkTile(const WorkerAssignmentManager& rAssignments, const Tile& rTile);

} // namespace ac

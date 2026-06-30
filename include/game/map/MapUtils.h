#pragma once

#include "game/map/Tile.h"
#include <cstdlib>
#include <utility>

namespace ac
{

// Calls fn(tile_ptr, distance) for every tile within Manhattan `radius` tiles of rOrigin.
// `distance` is the Manhattan distance from rOrigin to that tile (>= 0).
// When includeOrigin is false (the common case for aura scans), rOrigin itself is skipped.
// When includeOrigin is true (e.g. RecomputeMoistureInRadius needs to cover the changed tile
// itself), rOrigin is included at distance 0.
//
// WorldMapT can be WorldMap or const WorldMap — the tile pointer type in the callback matches
// automatically (Tile* from WorldMap&, const Tile* from const WorldMap&).
//
// Null tile pointers (map edge) are silently skipped before fn is invoked.
template<typename WorldMapT, typename Fn>
void ForEachTileInManhattanRadius(const Tile& rOrigin, WorldMapT& rWorldMap,
                                   int radius, bool includeOrigin, Fn&& fn)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            const int distance = std::abs(dx) + std::abs(dy);
            if (distance > radius) continue;
            if (!includeOrigin && distance == 0) continue;

            auto* pTile = rWorldMap.GetTile(rOrigin.GetX() + dx, rOrigin.GetY() + dy);
            if (pTile)
            {
                fn(pTile, distance);
            }
        }
    }
}

} // namespace ac

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

// Calls fn(tile_ptr) for every tile in the base workable area: a 5x5 square centred on
// rOrigin with the 4 corner tiles removed (20 tiles total). rOrigin itself is excluded.
// WorldMapT can be WorldMap or const WorldMap.
template<typename WorldMapT, typename Fn>
void ForEachTileInWorkableArea(const Tile& rOrigin, WorldMapT& rWorldMap, Fn&& fn)
{
    static constexpr int kRadius = 2;
    for (int dy = -kRadius; dy <= kRadius; ++dy)
    {
        for (int dx = -kRadius; dx <= kRadius; ++dx)
        {
            if (dx == 0 && dy == 0) continue;
            if (std::abs(dx) == kRadius && std::abs(dy) == kRadius) continue;

            auto* pTile = rWorldMap.GetTile(rOrigin.GetX() + dx, rOrigin.GetY() + dy);
            if (pTile)
            {
                fn(pTile);
            }
        }
    }
}

} // namespace ac

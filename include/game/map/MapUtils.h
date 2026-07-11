#pragma once

#include "game/map/Tile.h"
#include <algorithm>
#include <cstdlib>
#include <utility>

namespace ac
{

// Discrete Euclidean disk shared by base territory and the workable area:
// dx^2 + dy^2 <= radius^2 + 1. Radius 2 yields the classic SMAC 5x5-minus-corners
// workable cross (20 tiles around the base).
inline bool InEuclideanRadius(int dx, int dy, int radius)
{
    return dx * dx + dy * dy <= radius * radius + 1;
}

// Calls fn(tile_ptr, distance) for every tile within Chebyshev `radius` tiles of rOrigin
// (square / king-move distance: max(|dx|, |dy|)). This is the metric for vision and for
// all effect/improvement aura radii (Sensor, Condenser, unit auras, …).
// `distance` is the Chebyshev distance from rOrigin (>= 0).
// When includeOrigin is false (the common case for aura scans), rOrigin itself is skipped.
// When includeOrigin is true (e.g. RecomputeMoistureInRadius needs to cover the changed tile
// itself), rOrigin is included at distance 0.
//
// WorldMapT can be WorldMap or const WorldMap — the tile pointer type in the callback matches
// automatically (Tile* from WorldMap&, const Tile* from const WorldMap&).
//
// Null tile pointers (map edge) are silently skipped before fn is invoked.
template<typename WorldMapT, typename Fn>
void ForEachTileInChebyshevRadius(const Tile& rOrigin, WorldMapT& rWorldMap,
                                   int radius, bool includeOrigin, Fn&& fn)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            const int distance = std::max(std::abs(dx), std::abs(dy));
            if (distance > radius)
            {
                continue;
            }
            if (!includeOrigin && distance == 0)
            {
                continue;
            }

            auto* pTile = rWorldMap.GetTile(rOrigin.GetX() + dx, rOrigin.GetY() + dy);
            if (pTile)
            {
                fn(pTile, distance);
            }
        }
    }
}

// Calls fn(tile_ptr, distSq) for every tile in the discrete Euclidean disk
// dx^2 + dy^2 <= radius^2 + 1 (see InEuclideanRadius). `distSq` is dx^2 + dy^2.
template<typename WorldMapT, typename Fn>
void ForEachTileInEuclideanRadius(const Tile& rOrigin, WorldMapT& rWorldMap,
                                  const int radius, bool includeOrigin, Fn&& fn)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            if (!InEuclideanRadius(dx, dy, radius))
            {
                continue;
            }
            if (!includeOrigin && dx == 0 && dy == 0)
            {
                continue;
            }

            auto* pTile = rWorldMap.GetTile(rOrigin.GetX() + dx, rOrigin.GetY() + dy);
            if (pTile)
            {
                fn(pTile, dx * dx + dy * dy);
            }
        }
    }
}

// The SMAC base workable area: Euclidean radius 2 (dx^2 + dy^2 <= 5), which is a 5x5
// square with the four corners removed. Skips rOrigin itself (20 surrounding tiles).
// WorldMapT can be WorldMap or const WorldMap.
template<typename WorldMapT, typename Fn>
void ForEachTileInWorkableArea(const Tile& rOrigin, WorldMapT& rWorldMap, Fn&& fn)
{
    static constexpr int k_WorkableRadius = 2;
    ForEachTileInEuclideanRadius(rOrigin, rWorldMap, k_WorkableRadius, /*includeOrigin=*/false,
        [&](auto* pTile, int /*distSq*/) { fn(pTile); });
}

} // namespace ac

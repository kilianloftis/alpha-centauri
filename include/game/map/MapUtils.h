#pragma once

#include "game/map/Tile.h"
#include <algorithm>
#include <cstdlib>
#include <utility>

namespace ac
{

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

// The SMAC base workable cross: a 5×5 square centred on rOrigin with the 4 corner tiles
// removed (21 tiles including the base center). This iterator skips rOrigin itself and
// yields the 20 surrounding workable tiles. Not a Chebyshev/Manhattan disk — the cut
// corners are intentional. WorldMapT can be WorldMap or const WorldMap.
template<typename WorldMapT, typename Fn>
void ForEachTileInWorkableArea(const Tile& rOrigin, WorldMapT& rWorldMap, Fn&& fn)
{
    static constexpr int k_Radius = 2;
    for (int dy = -k_Radius; dy <= k_Radius; ++dy)
    {
        for (int dx = -k_Radius; dx <= k_Radius; ++dx)
        {
            if (dx == 0 && dy == 0) continue;
            if (std::abs(dx) == k_Radius && std::abs(dy) == k_Radius) continue;

            auto* pTile = rWorldMap.GetTile(rOrigin.GetX() + dx, rOrigin.GetY() + dy);
            if (pTile)
            {
                fn(pTile);
            }
        }
    }
}

} // namespace ac

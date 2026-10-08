#pragma once

#include "graphics/Graphics.h"

#include <utility>

namespace ac
{

class Tile;

inline constexpr float k_IsoHeightRatio = 0.5f;

template<typename T>
struct DiamondValues_t
{
    T center{};
    T west{};
    T north{};
    T east{};
    T south{};
};

// A tile and the shape a display draws it on.
struct PlacedTile_t
{
    const Tile* pTile = nullptr;
    TileShape_t shape;
};

TileShape_t FlatTileShape(float x, float y, float size);

bool ShapeContains(const TileShape_t& rShape, float px, float py);

std::pair<float, float> SeatOf(const TileShape_t& rShape);

// Top-left of the shape's flat footprint (its diamond's bounding box), seated at the mean of its
// four corners as SMAC seats everything on a tile.
std::pair<float, float> FootprintOrigin(const TileShape_t& rShape);

} // namespace ac

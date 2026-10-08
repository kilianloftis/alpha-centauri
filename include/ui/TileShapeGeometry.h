#pragma once

#include "graphics/Graphics.h"

#include <utility>

namespace ac
{

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

TileShape_t FlatTileShape(float x, float y, float size);

bool ShapeContains(const TileShape_t& rShape, float px, float py);

std::pair<float, float> SeatOf(const TileShape_t& rShape);

} // namespace ac

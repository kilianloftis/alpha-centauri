#include "ui/TileShapeGeometry.h"

namespace ac
{

namespace
{

float ShapeCross_(const TileVertex_t& rA, const TileVertex_t& rB, float px, float py)
{
    return (rB.x - rA.x) * (py - rA.y) - (rB.y - rA.y) * (px - rA.x);
}

bool ShapeTriangleContains_(const TileVertex_t& rA, const TileVertex_t& rB, const TileVertex_t& rC,
                            float px, float py)
{
    const float d1 = ShapeCross_(rA, rB, px, py);
    const float d2 = ShapeCross_(rB, rC, px, py);
    const float d3 = ShapeCross_(rC, rA, px, py);
    const bool bNegative = d1 < 0.0f || d2 < 0.0f || d3 < 0.0f;
    const bool bPositive = d1 > 0.0f || d2 > 0.0f || d3 > 0.0f;
    return !(bNegative && bPositive);
}

} // namespace

TileShape_t FlatTileShape(float x, float y, float size)
{
    const float width = size;
    const float height = size * k_IsoHeightRatio;
    const auto vertex = [&](float u, float v) {
        return TileVertex_t{x + width * u, y + height * v};
    };
    return TileShape_t{vertex(0.5f, 0.5f), vertex(0.0f, 0.5f), vertex(0.5f, 0.0f),
                       vertex(1.0f, 0.5f), vertex(0.5f, 1.0f)};
}

bool ShapeContains(const TileShape_t& rShape, float px, float py)
{
    const TileVertex_t& rC = rShape.center;
    return ShapeTriangleContains_(rC, rShape.west, rShape.north, px, py)
           || ShapeTriangleContains_(rC, rShape.north, rShape.east, px, py)
           || ShapeTriangleContains_(rC, rShape.east, rShape.south, px, py)
           || ShapeTriangleContains_(rC, rShape.south, rShape.west, px, py);
}

std::pair<float, float> SeatOf(const TileShape_t& rShape)
{
    const float seatX =
        (rShape.west.x + rShape.north.x + rShape.east.x + rShape.south.x) * 0.25f;
    const float seatY =
        (rShape.west.y + rShape.north.y + rShape.east.y + rShape.south.y) * 0.25f;
    return {seatX, seatY};
}

} // namespace ac

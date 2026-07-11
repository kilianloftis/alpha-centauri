#pragma once

#include <optional>
#include <utility>

namespace ac
{

// Converts pixel coordinates (e.g. from mouse clicks) to tile coordinates.
// Used by both WorldDisplay and BaseWorkableAreaDisplay.
class TileHitTester
{
public:
    // Hit-test the full world map grid.
    // gridOriginX/Y: top-left pixel position of the grid (same x,y passed to WorldDisplay::Render).
    // Returns world tile coordinates (col, row), or nullopt if outside the grid.
    static std::optional<std::pair<int, int>> HitTestWorldGrid(
        float mouseX, float mouseY,
        float gridOriginX, float gridOriginY,
        float tileSize,
        int mapWidth, int mapHeight);

    // Hit-test the base workable area (Euclidean radius 2: dx^2+dy^2 <= 5).
    // renderCenterX/Y: the center pixel position passed to BaseWorkableAreaDisplay::Render.
    // baseX/baseY: the base's world tile coordinates.
    // Returns world tile coordinates, or nullopt if outside the workable area.
    static std::optional<std::pair<int, int>> HitTestBaseWorkableArea(
        float mouseX, float mouseY,
        float renderCenterX, float renderCenterY,
        float tileSize,
        int baseX, int baseY);

private:
    // Check whether a relative offset (dx, dy from base) is within the workable disk
    static bool IsInWorkableDiamond_(int dx, int dy);
};

} // namespace ac

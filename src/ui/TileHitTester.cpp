#include "ui/TileHitTester.h"
#include <cmath>
#include <cstdlib>

namespace ac
{

std::optional<std::pair<int, int>> TileHitTester::HitTestWorldGrid(
    float mouseX, float mouseY,
    float gridOriginX, float gridOriginY,
    float tileSize,
    int mapWidth, int mapHeight)
{
    if (tileSize <= 0.0f)
    {
        return std::nullopt;
    }

    float relX = mouseX - gridOriginX;
    float relY = mouseY - gridOriginY;

    if (relX < 0.0f || relY < 0.0f)
    {
        return std::nullopt;
    }

    int col = static_cast<int>(relX / tileSize);
    int row = static_cast<int>(relY / tileSize);

    if (col < 0 || col >= mapWidth || row < 0 || row >= mapHeight)
    {
        return std::nullopt;
    }

    return std::make_pair(col, row);
}

std::optional<std::pair<int, int>> TileHitTester::HitTestBaseWorkableArea(
    float mouseX, float mouseY,
    float renderCenterX, float renderCenterY,
    float tileSize,
    int baseX, int baseY)
{
    if (tileSize <= 0.0f)
    {
        return std::nullopt;
    }

    // BaseWorkableAreaDisplay::Render places relative tile (dx, dy) with its
    // top-left corner at (renderCenterX + dx*tileSize, renderCenterY + dy*tileSize).
    // Invert that to find dx, dy from the mouse position.
    float relX = mouseX - renderCenterX;
    float relY = mouseY - renderCenterY;

    // Determine which relative cell the click falls in
    int dx = static_cast<int>(std::floor(relX / tileSize));
    int dy = static_cast<int>(std::floor(relY / tileSize));

    if (!IsInWorkableDiamond_(dx, dy))
    {
        return std::nullopt;
    }

    return std::make_pair(baseX + dx, baseY + dy);
}

bool TileHitTester::IsInWorkableDiamond_(int dx, int dy)
{
    // 5x5 grid with corners removed: |dx|+|dy| <= 3, and exclude center (0,0)
    if (dx == 0 && dy == 0)
    {
        return false;
    }
    if (std::abs(dx) > 2 || std::abs(dy) > 2)
    {
        return false;
    }
    return (std::abs(dx) + std::abs(dy)) <= 3;
}

} // namespace ac

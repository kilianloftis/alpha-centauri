#include "ui/TileHitTester.h"
#include "game/map/MapUtils.h"
#include <cmath>
#include <cstdlib>

namespace ac
{

namespace
{

constexpr int k_WorkableGridRadius       = 2;
constexpr int k_BaseCenterOffset         = 0;
constexpr float k_MinTileSize            = 0.0f;
constexpr float k_MinRelativeCoordinate  = 0.0f;

} // namespace

std::optional<std::pair<int, int>> TileHitTester::HitTestWorldGrid(
    float mouseX, float mouseY,
    float gridOriginX, float gridOriginY,
    float tileSize,
    int mapWidth, int mapHeight)
{
    if (tileSize <= k_MinTileSize)
    {
        return std::nullopt;
    }

    float relX = mouseX - gridOriginX;
    float relY = mouseY - gridOriginY;

    if (relX < k_MinRelativeCoordinate || relY < k_MinRelativeCoordinate)
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
    if (tileSize <= k_MinTileSize)
    {
        return std::nullopt;
    }

    float relX = mouseX - renderCenterX;
    float relY = mouseY - renderCenterY;

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
    if (dx == k_BaseCenterOffset && dy == k_BaseCenterOffset)
    {
        return false;
    }
    return InEuclideanRadius(dx, dy, k_WorkableGridRadius);
}

} // namespace ac

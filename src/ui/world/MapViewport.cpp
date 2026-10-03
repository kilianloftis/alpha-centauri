#include "ui/world/MapViewport.h"

#include "game/map/MapUtils.h"

#include <algorithm>
#include <cmath>

namespace ac
{

namespace
{

constexpr float k_IsoHeightRatio = 0.5f;

} // namespace

MapViewport::MapViewport(const WorldMap& rWorldMap, WindowLayout_t layout, float tileSize)
    : m_rWorldMap(rWorldMap)
    , m_layout(layout)
    , m_tileWidth(tileSize)
    , m_tileHeight(tileSize * k_IsoHeightRatio)
    , m_visibleCols(std::max(1, static_cast<int>(layout.width / tileSize)))
    , m_visibleRows(std::max(1, static_cast<int>(layout.height / (tileSize * k_IsoHeightRatio))))
{
}

bool MapViewport::SetCamera(int tileX, int tileY)
{
    const int mapWidth = m_rWorldMap.GetWidth();
    const int newX = mapWidth > 0 ? WrapX(tileX, mapWidth) : tileX;
    const int newY = tileY;
    if (newX == m_cameraX && newY == m_cameraY)
    {
        return false;
    }
    m_cameraX = newX;
    m_cameraY = newY;
    return true;
}

bool MapViewport::ScrollBy(int deltaX, int deltaY)
{
    return SetCamera(m_cameraX + deltaX, m_cameraY + deltaY);
}

int MapViewport::RowStart() const
{
    return std::max(0, m_cameraY);
}

int MapViewport::RowEnd() const
{
    return std::min(m_rWorldMap.GetHeight(), RowStart() + m_visibleRows);
}

int MapViewport::WrapWorldX_(int worldX) const
{
    const int mapWidth = m_rWorldMap.GetWidth();
    return mapWidth > 0 ? WrapX(worldX, mapWidth) : worldX;
}

void MapViewport::AabbOriginFromRel_(int relX, int relY, float& rOutX, float& rOutY) const
{
    const float halfW = m_tileWidth * 0.5f;
    const float halfH = m_tileHeight * 0.5f;
    rOutX = m_layout.x + static_cast<float>(relX - relY) * halfW;
    rOutY = m_layout.y + static_cast<float>(relX + relY) * halfH;
}

bool MapViewport::AabbIntersectsLayout_(float aabbX, float aabbY) const
{
    const float layoutRight = m_layout.x + m_layout.width;
    const float layoutBottom = m_layout.y + m_layout.height;
    return aabbX < layoutRight && aabbX + m_tileWidth > m_layout.x && aabbY < layoutBottom
           && aabbY + m_tileHeight > m_layout.y;
}

std::optional<std::pair<float, float>> MapViewport::PixelOriginOf(int worldX, int worldY) const
{
    const int mapWidth = m_rWorldMap.GetWidth();
    const int mapHeight = m_rWorldMap.GetHeight();
    if (mapWidth <= 0 || worldY < 0 || worldY >= mapHeight)
    {
        return std::nullopt;
    }

    // Shortest horizontal wrap so markers track the on-screen instance nearest the camera.
    int relX = worldX - m_cameraX;
    const int halfWidth = mapWidth / 2;
    while (relX > halfWidth)
    {
        relX -= mapWidth;
    }
    while (relX <= -halfWidth)
    {
        relX += mapWidth;
    }
    const int relY = worldY - m_cameraY;

    float aabbX = 0.0f;
    float aabbY = 0.0f;
    AabbOriginFromRel_(relX, relY, aabbX, aabbY);
    if (!AabbIntersectsLayout_(aabbX, aabbY))
    {
        return std::nullopt;
    }
    return std::pair{aabbX, aabbY};
}

std::optional<std::pair<float, float>> MapViewport::PixelCenterOf(const Tile& rTile) const
{
    const auto origin = PixelOriginOf(rTile.GetX(), rTile.GetY());
    if (!origin)
    {
        return std::nullopt;
    }
    return std::pair{origin->first + m_tileWidth * 0.5f, origin->second + m_tileHeight * 0.5f};
}

std::optional<std::pair<int, int>> MapViewport::WorldCoordsAtPixel(float pixelX, float pixelY) const
{
    const int mapWidth = m_rWorldMap.GetWidth();
    const int mapHeight = m_rWorldMap.GetHeight();
    if (mapWidth <= 0 || mapHeight <= 0 || m_tileWidth <= 0.0f || m_tileHeight <= 0.0f)
    {
        return std::nullopt;
    }
    if (pixelX < m_layout.x || pixelY < m_layout.y
        || pixelX >= m_layout.x + m_layout.width || pixelY >= m_layout.y + m_layout.height)
    {
        return std::nullopt;
    }

    // Unproject relative to diamond centers (AABB origin + half size).
    const float ux = pixelX - m_layout.x - m_tileWidth * 0.5f;
    const float uy = pixelY - m_layout.y - m_tileHeight * 0.5f;
    const float fRelX = ux / m_tileWidth + uy / m_tileHeight;
    const float fRelY = uy / m_tileHeight - ux / m_tileWidth;
    const int relX = static_cast<int>(std::lround(fRelX));
    const int relY = static_cast<int>(std::lround(fRelY));

    const int worldY = m_cameraY + relY;
    if (worldY < 0 || worldY >= mapHeight)
    {
        return std::nullopt;
    }
    return std::pair{WrapWorldX_(m_cameraX + relX), worldY};
}

} // namespace ac

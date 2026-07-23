#include "ui/world/MapViewport.h"

#include "game/map/MapUtils.h"

#include <algorithm>

namespace ac
{

MapViewport::MapViewport(const WorldMap& rWorldMap, WindowLayout_t layout, float tileSize)
    : m_rWorldMap(rWorldMap)
    , m_layout(layout)
    , m_tileSize(tileSize)
    , m_visibleCols(static_cast<int>(layout.width / tileSize))
    , m_visibleRows(static_cast<int>(layout.height / tileSize))
{
}

void MapViewport::SetCamera(int tileX, int tileY)
{
    const int mapWidth = m_rWorldMap.GetWidth();
    m_cameraX = mapWidth > 0 ? WrapX(tileX, mapWidth) : tileX;
    m_cameraY = tileY;
}

void MapViewport::ScrollBy(int deltaX, int deltaY)
{
    SetCamera(m_cameraX + deltaX, m_cameraY + deltaY);
}

int MapViewport::RowStart() const
{
    return std::max(0, m_cameraY);
}

int MapViewport::RowEnd() const
{
    return std::min(m_rWorldMap.GetHeight(), RowStart() + m_visibleRows);
}

int MapViewport::WorldXAt(int screenCol) const
{
    const int mapWidth = m_rWorldMap.GetWidth();
    return mapWidth > 0 ? WrapX(m_cameraX + screenCol, mapWidth) : m_cameraX + screenCol;
}

int MapViewport::WorldYAt(int screenRow) const
{
    return RowStart() + screenRow;
}

const Tile* MapViewport::TileAt(int screenCol, int screenRow) const
{
    return m_rWorldMap.GetTile(WorldXAt(screenCol), WorldYAt(screenRow));
}

std::optional<int> MapViewport::ScreenColOf(int worldX) const
{
    const int mapWidth = m_rWorldMap.GetWidth();
    if (mapWidth <= 0)
    {
        return std::nullopt;
    }
    const int screenCol = WrapX(worldX - m_cameraX, mapWidth);
    if (screenCol >= m_visibleCols)
    {
        return std::nullopt;
    }
    return screenCol;
}

bool MapViewport::ContainsWorldY(int worldY) const
{
    return worldY >= RowStart() && worldY < RowEnd();
}

bool MapViewport::IsInView(int worldX, int worldY) const
{
    return ScreenColOf(worldX).has_value() && ContainsWorldY(worldY);
}

float MapViewport::PixelX(int screenCol) const
{
    return m_layout.x + static_cast<float>(screenCol) * m_tileSize;
}

float MapViewport::PixelYForWorldY(int worldY) const
{
    return m_layout.y + static_cast<float>(worldY - RowStart()) * m_tileSize;
}

std::optional<std::pair<float, float>> MapViewport::PixelOriginOf(int worldX, int worldY) const
{
    const std::optional<int> screenCol = ScreenColOf(worldX);
    if (!screenCol || !ContainsWorldY(worldY))
    {
        return std::nullopt;
    }
    return std::pair{PixelX(*screenCol), PixelYForWorldY(worldY)};
}

std::optional<std::pair<float, float>> MapViewport::PixelCenterOf(const Tile& rTile) const
{
    const auto origin = PixelOriginOf(rTile.GetX(), rTile.GetY());
    if (!origin)
    {
        return std::nullopt;
    }
    const float half = m_tileSize * 0.5f;
    return std::pair{origin->first + half, origin->second + half};
}

std::optional<std::pair<int, int>> MapViewport::WorldCoordsAt(int screenCol, int screenRow) const
{
    if (screenCol < 0 || screenCol >= m_visibleCols
        || screenRow < 0 || screenRow >= m_visibleRows)
    {
        return std::nullopt;
    }
    const int worldY = WorldYAt(screenRow);
    if (worldY < 0 || worldY >= m_rWorldMap.GetHeight())
    {
        return std::nullopt;
    }
    return std::pair{WorldXAt(screenCol), worldY};
}

} // namespace ac

#include "ui/world/MapViewport.h"

#include "game/map/MapUtils.h"

#include <algorithm>
#include <cmath>

namespace ac
{

namespace
{

constexpr float k_IsoHeightRatio = 0.5f;

float Cross_(const TileVertex_t& rA, const TileVertex_t& rB, float px, float py)
{
    return (rB.x - rA.x) * (py - rA.y) - (rB.y - rA.y) * (px - rA.x);
}

bool TriangleContains_(const TileVertex_t& rA, const TileVertex_t& rB, const TileVertex_t& rC,
                       float px, float py)
{
    const float d1 = Cross_(rA, rB, px, py);
    const float d2 = Cross_(rB, rC, px, py);
    const float d3 = Cross_(rC, rA, px, py);
    const bool bNegative = d1 < 0.0f || d2 < 0.0f || d3 < 0.0f;
    const bool bPositive = d1 > 0.0f || d2 > 0.0f || d3 > 0.0f;
    return !(bNegative && bPositive);
}

bool ShapeContains_(const TileShape_t& rShape, float px, float py)
{
    const TileVertex_t& rC = rShape.center;
    return TriangleContains_(rC, rShape.west, rShape.north, px, py)
           || TriangleContains_(rC, rShape.north, rShape.east, px, py)
           || TriangleContains_(rC, rShape.east, rShape.south, px, py)
           || TriangleContains_(rC, rShape.south, rShape.west, px, py);
}

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

void MapViewport::SetRelief(ReliefMode_t mode, const ReliefStyle_t& rStyle)
{
    m_relief = mode;
    m_reliefStyle = rStyle;
}

int MapViewport::WrapWorldX_(int worldX) const
{
    const int mapWidth = m_rWorldMap.GetWidth();
    return mapWidth > 0 ? WrapX(worldX, mapWidth) : worldX;
}

std::optional<std::pair<int, int>> MapViewport::RelOf_(int worldX, int worldY) const
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
    return std::pair{relX, worldY - m_cameraY};
}

void MapViewport::AabbOriginFromRel_(int relX, int relY, float& rOutX, float& rOutY) const
{
    const float halfW = m_tileWidth * 0.5f;
    const float halfH = m_tileHeight * 0.5f;
    rOutX = m_layout.x + static_cast<float>(relX - relY) * halfW;
    rOutY = m_layout.y + static_cast<float>(relX + relY) * halfH;
}

bool MapViewport::BoxIntersectsLayout_(float x, float y, float height) const
{
    const float layoutRight = m_layout.x + m_layout.width;
    const float layoutBottom = m_layout.y + m_layout.height;
    return x < layoutRight && x + m_tileWidth > m_layout.x && y < layoutBottom
           && y + height > m_layout.y;
}

bool MapViewport::ShapeIntersectsLayout_(const TileShape_t& rShape) const
{
    const float top =
        std::min({rShape.center.y, rShape.west.y, rShape.north.y, rShape.east.y, rShape.south.y});
    const float bottom =
        std::max({rShape.center.y, rShape.west.y, rShape.north.y, rShape.east.y, rShape.south.y});
    return BoxIntersectsLayout_(rShape.west.x, top, bottom - top);
}

TileShape_t MapViewport::ShapeAt_(const Tile& rTile, float aabbX, float aabbY,
                                  bool bShaded) const
{
    const TileLifts_t lifts = ResolveTileLifts(rTile, m_rWorldMap, m_relief, m_reliefStyle);
    const TileShades_t shades =
        bShaded ? ResolveTileShades(rTile, m_rWorldMap, m_relief, m_reliefStyle) : TileShades_t{};
    const auto vertex = [&](float u, float v, float lift, float shade) {
        return TileVertex_t{aabbX + m_tileWidth * u, aabbY + m_tileHeight * v - lift * m_tileWidth,
                            shade};
    };
    return TileShape_t{
        vertex(0.5f, 0.5f, lifts.center, shades.center),
        vertex(0.0f, 0.5f, lifts.west, shades.west),
        vertex(0.5f, 0.0f, lifts.north, shades.north),
        vertex(1.0f, 0.5f, lifts.east, shades.east),
        vertex(0.5f, 1.0f, lifts.south, shades.south),
    };
}

float MapViewport::MaxLiftPixels_() const
{
    const Tile* pAnyTile = m_rWorldMap.GetTile(0, 0);
    if (!pAnyTile)
    {
        return 0.0f;
    }
    return MaxTileLift(pAnyTile->MapRules(), m_relief, m_reliefStyle) * m_tileWidth;
}

std::optional<std::pair<float, float>> MapViewport::PixelOriginOf(int worldX, int worldY) const
{
    const auto rel = RelOf_(worldX, worldY);
    const Tile* pTile = m_rWorldMap.GetTile(worldX, worldY);
    if (!rel || !pTile)
    {
        return std::nullopt;
    }

    float aabbX = 0.0f;
    float aabbY = 0.0f;
    AabbOriginFromRel_(rel->first, rel->second, aabbX, aabbY);
    const float lift = ResolveTileLifts(*pTile, m_rWorldMap, m_relief, m_reliefStyle).center;
    const float originY = aabbY - lift * m_tileWidth;
    if (!BoxIntersectsLayout_(aabbX, originY, m_tileHeight))
    {
        return std::nullopt;
    }
    return std::pair{aabbX, originY};
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
    const int flatRelX = static_cast<int>(std::lround(fRelX));
    const int flatRelY = static_cast<int>(std::lround(fRelY));

    // A raised tile shows above its flat place, so the pixel may belong to a tile further down
    // the screen. Search from the front (largest depth) back to the flat tile's row.
    const int reach =
        static_cast<int>(std::ceil(MaxLiftPixels_() / (m_tileHeight * 0.5f))) + 1;
    const int flatDepth = flatRelX + flatRelY;
    const int flatColumn = flatRelX - flatRelY;
    for (int depth = flatDepth + reach; depth >= flatDepth - 1; --depth)
    {
        for (int column = flatColumn - 1; column <= flatColumn + 1; ++column)
        {
            if (((depth + column) & 1) != 0)
            {
                continue;
            }
            const int relX = (depth + column) / 2;
            const int relY = (depth - column) / 2;
            const int worldY = m_cameraY + relY;
            const Tile* pTile = m_rWorldMap.GetTile(WrapWorldX_(m_cameraX + relX), worldY);
            if (!pTile)
            {
                continue;
            }
            float aabbX = 0.0f;
            float aabbY = 0.0f;
            AabbOriginFromRel_(relX, relY, aabbX, aabbY);
            if (ShapeContains_(ShapeAt_(*pTile, aabbX, aabbY, /*bShaded*/ false), pixelX, pixelY))
            {
                return std::pair{WrapWorldX_(m_cameraX + relX), worldY};
            }
        }
    }

    const int worldY = m_cameraY + flatRelY;
    if (worldY < 0 || worldY >= mapHeight)
    {
        return std::nullopt;
    }
    return std::pair{WrapWorldX_(m_cameraX + flatRelX), worldY};
}

} // namespace ac

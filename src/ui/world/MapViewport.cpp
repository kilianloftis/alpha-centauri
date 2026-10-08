#include "ui/world/MapViewport.h"

#include "game/map/MapUtils.h"
#include "ui/TileRelief.h"
#include "ui/TileShapeGeometry.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ac
{

namespace
{

// Flat diamond under map units (u, v): centre (x+1, y+1) with |u-x-1|+|v-y-1| ≤ 1.
std::optional<std::pair<int, int>> FlatTileAtMapUnits_(float u, float v, int mapWidth,
                                                       int mapHeight)
{
    const int approxX = static_cast<int>(std::floor(u)) - 1;
    const int approxY = static_cast<int>(std::floor(v)) - 1;
    std::optional<std::pair<int, int>> best;
    float bestDist = 0.0f;
    for (int dy = -1; dy <= 2; ++dy)
    {
        for (int dx = -1; dx <= 2; ++dx)
        {
            const int x = approxX + dx;
            const int y = approxY + dy;
            if (y < 0 || y >= mapHeight || ((x + y) & 1) != 0)
            {
                continue;
            }
            const float manhattan =
                std::abs(u - static_cast<float>(x) - 1.0f)
                + std::abs(v - static_cast<float>(y) - 1.0f);
            if (manhattan > 1.0f + 1e-4f)
            {
                continue;
            }
            if (!best || manhattan < bestDist)
            {
                best = std::pair{WrapX(x, mapWidth), y};
                bestDist = manhattan;
            }
        }
    }
    return best;
}

bool ReliefStyleEqual_(const ReliefStyle_t& rLeft, const ReliefStyle_t& rRight)
{
    return rLeft.liftPerLevelRatio == rRight.liftPerLevelRatio
           && rLeft.levelMeters == rRight.levelMeters
           && rLeft.fullShadeRiseMeters == rRight.fullShadeRiseMeters
           && rLeft.altitudeLightSteps == rRight.altitudeLightSteps;
}

float MaxDiamondLift_(const TileLifts_t& rLifts)
{
    return std::max({rLifts.center, rLifts.west, rLifts.north, rLifts.east, rLifts.south});
}

} // namespace

MapViewport::MapViewport(const WorldMap& rWorldMap, WindowLayout_t layout, float tileSize)
    : m_rWorldMap(rWorldMap)
    , m_layout(layout)
    , m_tileWidth(tileSize)
    , m_tileHeight(tileSize * k_IsoHeightRatio)
    , m_visibleCols(std::max(1, static_cast<int>(layout.width / (tileSize * 0.5f))))
    , m_visibleRows(
          std::max(1, static_cast<int>(layout.height / (tileSize * k_IsoHeightRatio * 0.5f))))
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

bool MapViewport::SetTileSize(float tileSize)
{
    if (!(tileSize > 0.0f))
    {
        throw std::runtime_error("MapViewport::SetTileSize: tileSize must be positive");
    }
    if (tileSize == m_tileWidth)
    {
        return false;
    }
    m_tileWidth = tileSize;
    m_tileHeight = tileSize * k_IsoHeightRatio;
    m_visibleCols = std::max(1, static_cast<int>(m_layout.width / (tileSize * 0.5f)));
    m_visibleRows =
        std::max(1, static_cast<int>(m_layout.height / (tileSize * k_IsoHeightRatio * 0.5f)));
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
    rOutX = m_layout.x + static_cast<float>(relX) * halfW;
    rOutY = m_layout.y + static_cast<float>(relY) * halfH;
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

void MapViewport::EnsureReliefCache_() const
{
    const uint64_t appearanceRevision = m_rWorldMap.GetAppearanceRevision();
    if (m_bReliefCacheValid && appearanceRevision == m_reliefCacheAppearanceRevision
        && m_relief == m_reliefCacheMode && ReliefStyleEqual_(m_reliefStyle, m_reliefCacheStyle))
    {
        return;
    }

    const std::span<const std::unique_ptr<Tile>> tiles = m_rWorldMap.GetTiles();
    m_reliefLifts.resize(tiles.size());
    m_reliefShades.resize(tiles.size());
    float maxLiftRatio = 0.0f;
    for (const std::unique_ptr<Tile>& pTile : tiles)
    {
        const int index = m_rWorldMap.GetTileIndex(*pTile);
        m_reliefLifts[static_cast<std::size_t>(index)] =
            ResolveTileLifts(*pTile, m_rWorldMap, m_relief, m_reliefStyle);
        m_reliefShades[static_cast<std::size_t>(index)] =
            ResolveTileShades(*pTile, m_rWorldMap, m_relief, m_reliefStyle);
        maxLiftRatio =
            std::max(maxLiftRatio, MaxDiamondLift_(m_reliefLifts[static_cast<std::size_t>(index)]));
    }
    m_cachedMaxLiftRatio = maxLiftRatio;
    m_reliefCacheAppearanceRevision = appearanceRevision;
    m_reliefCacheMode = m_relief;
    m_reliefCacheStyle = m_reliefStyle;
    m_bReliefCacheValid = true;
}

const TileLifts_t& MapViewport::CachedLifts_(const Tile& rTile) const
{
    EnsureReliefCache_();
    return m_reliefLifts[static_cast<std::size_t>(m_rWorldMap.GetTileIndex(rTile))];
}

const TileShades_t& MapViewport::CachedShades_(const Tile& rTile) const
{
    EnsureReliefCache_();
    return m_reliefShades[static_cast<std::size_t>(m_rWorldMap.GetTileIndex(rTile))];
}

TileShape_t MapViewport::ShapeAt_(const Tile& rTile, float aabbX, float aabbY,
                                  bool bShaded) const
{
    const TileLifts_t& lifts = CachedLifts_(rTile);
    const TileShades_t shades = bShaded ? CachedShades_(rTile) : TileShades_t{};
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
    EnsureReliefCache_();
    return m_cachedMaxLiftRatio * m_tileWidth;
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
    // SMAC seats a tile's contents at the mean of its four corner lifts (MapWin_tile_to_pixel).
    const TileLifts_t& lifts = CachedLifts_(*pTile);
    const float seatLift = (lifts.west + lifts.north + lifts.east + lifts.south) * 0.25f;
    const float originY = aabbY - seatLift * m_tileWidth;
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

    const float halfW = m_tileWidth * 0.5f;
    const float halfH = m_tileHeight * 0.5f;
    const float u = static_cast<float>(m_cameraX) + (pixelX - m_layout.x) / halfW;
    const float v = static_cast<float>(m_cameraY) + (pixelY - m_layout.y) / halfH;

    const auto flat = FlatTileAtMapUnits_(u, v, mapWidth, mapHeight);
    if (!flat)
    {
        return std::nullopt;
    }

    // Raised tiles show above their flat place; search front-first (larger y) back to the flat
    // tile's row.
    const int reach =
        static_cast<int>(std::ceil(MaxLiftPixels_() / halfH)) + 1;
    const int flatY = flat->second;
    for (int worldY = std::min(mapHeight - 1, flatY + reach); worldY >= flatY; --worldY)
    {
        for (int dx = -2; dx <= 2; ++dx)
        {
            const int worldX = WrapWorldX_(flat->first + dx);
            if (((worldX + worldY) & 1) != 0)
            {
                continue;
            }
            const Tile* pTile = m_rWorldMap.GetTile(worldX, worldY);
            if (!pTile)
            {
                continue;
            }
            const auto rel = RelOf_(worldX, worldY);
            if (!rel)
            {
                continue;
            }
            float aabbX = 0.0f;
            float aabbY = 0.0f;
            AabbOriginFromRel_(rel->first, rel->second, aabbX, aabbY);
            if (ShapeContains(ShapeAt_(*pTile, aabbX, aabbY, /*bShaded*/ false), pixelX, pixelY))
            {
                return std::pair{worldX, worldY};
            }
        }
    }

    return flat;
}

} // namespace ac

#pragma once

#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "ui/UIElement.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace ac
{

// Camera window over a WorldMap that wraps horizontally (cylinder). Presentation is a
// SMAC-style 2:1 isometric diamond grid; gameplay topology stays square.
class MapViewport
{
public:
    // tileSize is the diamond width in pixels; height is width / 2.
    MapViewport(const WorldMap& rWorldMap, WindowLayout_t layout, float tileSize);

    // Returns true when the camera position actually changed.
    bool SetCamera(int tileX, int tileY);
    bool ScrollBy(int deltaX, int deltaY);

    int CameraX() const { return m_cameraX; }
    int CameraY() const { return m_cameraY; }
    // Approximate orthogonal FOV used for camera centering and the top-down minimap frame.
    int VisibleCols() const { return m_visibleCols; }
    int VisibleRows() const { return m_visibleRows; }
    float TileSize() const { return m_tileWidth; }
    float TileWidth() const { return m_tileWidth; }
    float TileHeight() const { return m_tileHeight; }
    const WindowLayout_t& Layout() const { return m_layout; }
    const WorldMap& GetWorldMap() const { return m_rWorldMap; }

    // Approximate visible Y range for the minimap camera rectangle (Y does not wrap).
    int RowStart() const;
    int RowEnd() const;

    // Top-left of the diamond's axis-aligned bounding box, if any camera-relative
    // wrap instance intersects the layout.
    std::optional<std::pair<float, float>> PixelOriginOf(int worldX, int worldY) const;
    std::optional<std::pair<float, float>> PixelCenterOf(const Tile& rTile) const;

    // Inverse isometric hit-test: layout-relative pixel -> world tile (wrap-X applied).
    std::optional<std::pair<int, int>> WorldCoordsAtPixel(float pixelX, float pixelY) const;

    // fn(const Tile& tile, float pixelX, float pixelY) for every diamond whose AABB
    // intersects the layout, back-to-front (increasing relX+relY, then relX).
    template<typename Fn>
    void ForEachVisibleTile(Fn&& fn) const
    {
        struct Item_t
        {
            const Tile* pTile = nullptr;
            float pixelX = 0.0f;
            float pixelY = 0.0f;
            int depth = 0;
            int relX = 0;
        };

        std::vector<Item_t> items;
        const int mapWidth = m_rWorldMap.GetWidth();
        const int mapHeight = m_rWorldMap.GetHeight();
        if (mapWidth <= 0 || mapHeight <= 0 || m_tileWidth <= 0.0f || m_tileHeight <= 0.0f)
        {
            return;
        }

        const float halfW = m_tileWidth * 0.5f;
        const float halfH = m_tileHeight * 0.5f;
        const int range =
            static_cast<int>(std::ceil(m_layout.width / halfW + m_layout.height / halfH)) + 2;

        for (int relY = -range; relY <= range; ++relY)
        {
            const int worldY = m_cameraY + relY;
            if (worldY < 0 || worldY >= mapHeight)
            {
                continue;
            }
            for (int relX = -range; relX <= range; ++relX)
            {
                float aabbX = 0.0f;
                float aabbY = 0.0f;
                AabbOriginFromRel_(relX, relY, aabbX, aabbY);
                if (!AabbIntersectsLayout_(aabbX, aabbY))
                {
                    continue;
                }
                const int worldX = WrapWorldX_(m_cameraX + relX);
                const Tile* pTile = m_rWorldMap.GetTile(worldX, worldY);
                if (!pTile)
                {
                    continue;
                }
                items.push_back(Item_t{pTile, aabbX, aabbY, relX + relY, relX});
            }
        }

        std::sort(items.begin(), items.end(), [](const Item_t& a, const Item_t& b) {
            if (a.depth != b.depth)
            {
                return a.depth < b.depth;
            }
            return a.relX < b.relX;
        });

        for (const Item_t& rItem : items)
        {
            fn(*rItem.pTile, rItem.pixelX, rItem.pixelY);
        }
    }

private:
    int WrapWorldX_(int worldX) const;
    void AabbOriginFromRel_(int relX, int relY, float& rOutX, float& rOutY) const;
    bool AabbIntersectsLayout_(float aabbX, float aabbY) const;

    const WorldMap& m_rWorldMap;
    WindowLayout_t m_layout;
    float m_tileWidth = 0.0f;
    float m_tileHeight = 0.0f;
    int m_visibleCols = 0;
    int m_visibleRows = 0;
    int m_cameraX = 0;
    int m_cameraY = 0;
};

} // namespace ac

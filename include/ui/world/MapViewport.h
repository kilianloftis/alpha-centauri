#pragma once

#include "game/MapDisplayConfig.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"
#include "ui/TileRelief.h"
#include "ui/UIElement.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace ac
{

// Camera window over a WorldMap that wraps horizontally (cylinder). Presentation is a
// SMAC-style 2:1 isometric diamond grid whose tiles rise with the terrain (TileRelief);
// gameplay topology stays square.
class MapViewport
{
public:
    // tileSize is the diamond width in pixels; height is width / 2.
    MapViewport(const WorldMap& rWorldMap, WindowLayout_t layout, float tileSize);

    // Elevation display for every shape and position this viewport reports. Flat until set.
    void SetRelief(ReliefMode_t mode, const ReliefStyle_t& rStyle);

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

    // Top-left of the tile's footprint (the diamond's bounding box at its raised centre), if
    // the camera-relative wrap instance nearest the camera intersects the layout.
    std::optional<std::pair<float, float>> PixelOriginOf(int worldX, int worldY) const;
    // The tile's raised centre.
    std::optional<std::pair<float, float>> PixelCenterOf(const Tile& rTile) const;

    // Inverse projection: the frontmost tile whose raised shape contains the pixel (wrap-X
    // applied).
    std::optional<std::pair<int, int>> WorldCoordsAtPixel(float pixelX, float pixelY) const;

    // Top-left of the tile's flat footprint (diamond bounding box) at the shape's raised centre.
    std::pair<float, float> FootprintOrigin(const TileShape_t& rShape) const
    {
        return {rShape.west.x, rShape.center.y - m_tileHeight * 0.5f};
    }

    // fn(const Tile& tile, const TileShape_t& shape) for every tile whose raised shape reaches
    // the layout, back-to-front (increasing relX+relY, then relX). bShaded adds the relief's
    // slope shades, which only terrain drawing needs.
    template<typename Fn>
    void ForEachVisibleTile(Fn&& fn, bool bShaded = true) const
    {
        struct Item_t
        {
            const Tile* pTile = nullptr;
            TileShape_t shape;
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
        // Tiles below the layout can rise into it.
        const float maxLift = MaxLiftPixels_();
        const int range = static_cast<int>(std::ceil(m_layout.width / halfW
                                                     + (m_layout.height + maxLift) / halfH))
                          + 2;

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
                if (!BoxIntersectsLayout_(aabbX, aabbY - maxLift, m_tileHeight + maxLift))
                {
                    continue;
                }
                const int worldX = WrapWorldX_(m_cameraX + relX);
                const Tile* pTile = m_rWorldMap.GetTile(worldX, worldY);
                if (!pTile)
                {
                    continue;
                }
                TileShape_t shape = ShapeAt_(*pTile, aabbX, aabbY, bShaded);
                if (!ShapeIntersectsLayout_(shape))
                {
                    continue;
                }
                items.push_back(Item_t{pTile, shape, relX + relY, relX});
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
            fn(*rItem.pTile, rItem.shape);
        }
    }

private:
    int WrapWorldX_(int worldX) const;
    // relX/relY of the camera-relative wrap instance nearest the camera.
    std::optional<std::pair<int, int>> RelOf_(int worldX, int worldY) const;
    void AabbOriginFromRel_(int relX, int relY, float& rOutX, float& rOutY) const;
    bool BoxIntersectsLayout_(float x, float y, float height) const;
    bool ShapeIntersectsLayout_(const TileShape_t& rShape) const;
    // The tile raised by the relief, with its flat diamond box at (aabbX, aabbY); bShaded adds
    // the slope shades.
    TileShape_t ShapeAt_(const Tile& rTile, float aabbX, float aabbY, bool bShaded) const;
    // How far the map's highest tile can rise, in pixels.
    float MaxLiftPixels_() const;

    const WorldMap& m_rWorldMap;
    WindowLayout_t m_layout;
    float m_tileWidth = 0.0f;
    float m_tileHeight = 0.0f;
    int m_visibleCols = 0;
    int m_visibleRows = 0;
    int m_cameraX = 0;
    int m_cameraY = 0;
    ReliefMode_t m_relief = ReliefMode_t::Flat;
    ReliefStyle_t m_reliefStyle{};
};

} // namespace ac

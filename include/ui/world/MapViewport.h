#pragma once

#include "game/MapDisplayConfig.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"
#include "ui/TileRelief.h"
#include "ui/TileShapeGeometry.h"
#include "ui/UIElement.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace ac
{

// Camera window over a WorldMap that wraps horizontally (cylinder). Presentation is a
// SMAC-style rectangular brick of diamonds: tile (x, y) sits at ((x-camX)·½w, (y-camY)·½h).
// Gameplay topology stays the square lattice.
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

    // Diamond width in pixels; height stays width / 2. Recalculates VisibleCols / VisibleRows.
    // Returns true when the size actually changed.
    bool SetTileSize(float tileSize);

    int CameraX() const { return m_cameraX; }
    int CameraY() const { return m_cameraY; }
    // Layout size in whole map units (½ tile width / height each).
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

    // Top-left of the tile's footprint (the diamond's bounding box seated at the mean of its
    // four corner lifts), if the camera-relative wrap instance nearest the camera intersects the
    // layout.
    std::optional<std::pair<float, float>> PixelOriginOf(int worldX, int worldY) const;
    // The centre of that footprint.
    std::optional<std::pair<float, float>> PixelCenterOf(const Tile& rTile) const;

    // Inverse projection: the frontmost tile whose raised shape contains the pixel (wrap-X
    // applied).
    std::optional<std::pair<int, int>> WorldCoordsAtPixel(float pixelX, float pixelY) const;

    // Top-left of the tile's flat footprint (diamond bounding box), seated at the mean of the
    // shape's four corners as SMAC seats everything on a tile.
    std::pair<float, float> FootprintOrigin(const TileShape_t& rShape) const
    {
        const auto [seatX, seatY] = SeatOf(rShape);
        (void)seatX;
        return {rShape.west.x, seatY - m_tileHeight * 0.5f};
    }

    // fn(const Tile& tile, const TileShape_t& shape) for every tile whose raised shape reaches
    // the layout, back-to-front (north row first). bShaded adds the relief's slope shades,
    // which only terrain drawing needs.
    template<typename Fn>
    void ForEachVisibleTile(Fn&& fn, bool bShaded = true) const
    {
        const int mapWidth = m_rWorldMap.GetWidth();
        const int mapHeight = m_rWorldMap.GetHeight();
        if (mapWidth <= 0 || mapHeight <= 0 || m_tileWidth <= 0.0f || m_tileHeight <= 0.0f)
        {
            return;
        }

        const float halfW = m_tileWidth * 0.5f;
        const float halfH = m_tileHeight * 0.5f;
        const float maxLift = MaxLiftPixels_();
        // One row above the camera; past the layout bottom by how far a raised tile can climb.
        const int relYStart = -1;
        const int relYEnd =
            static_cast<int>(std::ceil((m_layout.height + maxLift) / halfH)) + 1;
        const int relXStart = -2;
        const int relXEnd = static_cast<int>(std::ceil(m_layout.width / halfW)) + 2;

        for (int relY = relYStart; relY <= relYEnd; ++relY)
        {
            const int worldY = m_cameraY + relY;
            if (worldY < 0 || worldY >= mapHeight)
            {
                continue;
            }
            for (int relX = relXStart; relX <= relXEnd; ++relX)
            {
                const int worldX = WrapWorldX_(m_cameraX + relX);
                if (((worldX + worldY) & 1) != 0)
                {
                    continue;
                }
                float aabbX = 0.0f;
                float aabbY = 0.0f;
                AabbOriginFromRel_(relX, relY, aabbX, aabbY);
                if (!BoxIntersectsLayout_(aabbX, aabbY - maxLift, m_tileHeight + maxLift))
                {
                    continue;
                }
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
                fn(*pTile, shape);
            }
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

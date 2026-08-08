#pragma once

#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "ui/UIElement.h"

#include <optional>
#include <utility>

namespace ac
{

// Camera window over a WorldMap that wraps horizontally (cylinder). All east/west wrap
// math lives here — callers work in screen cells or world tiles and never call WrapX.
class MapViewport
{
public:
    MapViewport(const WorldMap& rWorldMap, WindowLayout_t layout, float tileSize);

    void SetCamera(int tileX, int tileY);
    void ScrollBy(int deltaX, int deltaY);

    int CameraX() const { return m_cameraX; }
    int CameraY() const { return m_cameraY; }
    int VisibleCols() const { return m_visibleCols; }
    int VisibleRows() const { return m_visibleRows; }
    float TileSize() const { return m_tileSize; }
    const WindowLayout_t& Layout() const { return m_layout; }
    const WorldMap& GetWorldMap() const { return m_rWorldMap; }

    // Visible Y range on the map (Y does not wrap).
    int RowStart() const;
    int RowEnd() const;

    // World coords / tile for a cell relative to the camera (screenCol in [0, VisibleCols)).
    int WorldXAt(int screenCol) const;
    int WorldYAt(int screenRow) const;
    const Tile* TileAt(int screenCol, int screenRow) const;

    // Screen cell for a world tile if it falls in the current camera window.
    std::optional<int> ScreenColOf(int worldX) const;
    bool ContainsWorldY(int worldY) const;

    float PixelX(int screenCol) const;
    float PixelYForWorldY(int worldY) const;
    std::optional<std::pair<float, float>> PixelOriginOf(int worldX, int worldY) const;
    std::optional<std::pair<float, float>> PixelCenterOf(const Tile& rTile) const;

    // Hit-test cell (from TileHitTester) -> canonical world tile coords.
    std::optional<std::pair<int, int>> WorldCoordsAt(int screenCol, int screenRow) const;

    // fn(const Tile& tile, float pixelX, float pixelY) for every cell in the camera window.
    template<typename Fn>
    void ForEachVisibleTile(Fn&& fn) const
    {
        const int rowStart = RowStart();
        const int rowEnd = RowEnd();
        for (int row = rowStart; row < rowEnd; ++row)
        {
            for (int screenCol = 0; screenCol < m_visibleCols; ++screenCol)
            {
                const Tile* pTile = TileAt(screenCol, row - rowStart);
                if (!pTile)
                {
                    continue;
                }
                fn(*pTile, PixelX(screenCol), PixelYForWorldY(row));
            }
        }
    }

private:
    const WorldMap& m_rWorldMap;
    WindowLayout_t m_layout;
    float m_tileSize = 0.0f;
    int m_visibleCols = 0;
    int m_visibleRows = 0;
    int m_cameraX = 0;
    int m_cameraY = 0;
};

} // namespace ac

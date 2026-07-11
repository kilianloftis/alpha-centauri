#pragma once

#include "graphics/Graphics.h"
#include "ui/UIElement.h"

namespace ac
{

class GameState;
class Unit;
class WorldMap;
class Tile;
enum class Moisture_t;
enum class Rockiness_t;

// Displays the world map as a grid of tiles.
// Each tile shows: moisture rockiness elevation(km). Bases and units are read
// live from GameState / WorldMap — no per-frame DTO rebuild.
class WorldDisplay
{
public:
    WorldDisplay(const GameState& rGameState, WindowLayout_t layout);

    // Set the unit currently selected by the player (highlighted on the map)
    void SetSelectedUnit(const Unit* pUnit);

    // Set the top-left tile coordinate of the visible viewport
    void SetCameraOffset(int tileX, int tileY);

    float GetEffectiveTileSize() const;
    int GetCameraX() const;
    int GetCameraY() const;
    int GetVisibleCols() const;
    int GetVisibleRows() const;

    // Render the world map using the stored layout.
    void Render(Graphics& rGraphics);

private:
    const GameState& m_rGameState;
    const Unit* m_pSelectedUnit = nullptr;

    WindowLayout_t m_layout;
    float m_tileSize = 0.0f;
    float m_effectiveTileSize = 40.0f;
    int m_visibleCols = 0;
    int m_visibleRows = 0;
    int m_cameraX = 0;
    int m_cameraY = 0;

    const WorldMap& GetWorldMap_() const;

    // Convert terrain enums to display integers
    int MoistureToInt_(Moisture_t moisture) const;
    int RockinessToInt_(Rockiness_t rockiness) const;

    // Render a single tile. Fogged tiles show terrain in muted color (explored memory).
    void RenderTile_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size,
                     bool bFogged = false);

    // Render base markers with owner color and population info
    void RenderBases_(Graphics& rGraphics, int colStart, int rowStart, int colEnd, int rowEnd);

    // Render unit markers on top of bases
    void RenderUnits_(Graphics& rGraphics, int colStart, int rowStart, int colEnd, int rowEnd);
};

} // namespace ac

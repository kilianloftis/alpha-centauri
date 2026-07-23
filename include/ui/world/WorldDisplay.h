#pragma once

#include "graphics/Graphics.h"
#include "game/units/Unit.h"
#include "ui/UIElement.h"
#include "ui/world/MapViewport.h"
#include "ui/world/UnitMarkerRenderer.h"

#include <optional>

namespace ac
{

class GameState;
class WorldMap;
class Tile;
struct Path_t;

// Displays the world map as a grid of tiles.
// Each tile shows: moisture rockiness elevation(km). Bases, Sensors, and units are read
// live from GameState / WorldMap — no per-frame DTO rebuild.
class WorldDisplay
{
public:
    WorldDisplay(const GameState& rGameState, WindowLayout_t layout);

    // Set the unit currently selected by the player (highlighted on the map). Also used as
    // the path-preview line origin when a path is active.
    void SetSelectedUnit(const Unit* pUnit);

    // Set the path preview to render (nullptr to clear). Pointer must remain valid until
    // the next Render call.
    void SetPathPreview(const Path_t* pPath);

    // Set the top-left tile coordinate of the visible viewport (X wraps).
    void SetCameraOffset(int tileX, int tileY);

    MapViewport& GetViewport() { return m_viewport; }
    const MapViewport& GetViewport() const { return m_viewport; }

    float GetEffectiveTileSize() const;
    int GetCameraX() const;
    int GetCameraY() const;
    int GetVisibleCols() const;
    int GetVisibleRows() const;

    // Unit marker layer: last-frame draw cache for combat overlays / hit animations.
    const UnitMarkerRenderer& GetUnitMarkers() const { return m_unitMarkers; }

    // Render the world map using the stored layout.
    void Render(Graphics& rGraphics);

private:
    const GameState& m_rGameState;
    const Unit* m_pSelectedUnit = nullptr;
    const Path_t* m_pPathPreview = nullptr;
    UnitMarkerRenderer m_unitMarkers;
    MapViewport m_viewport;

    // Render base markers with owner color and population info
    void RenderBases_(Graphics& rGraphics);

    // Render Sensor tower markers on explored tiles
    void RenderSensors_(Graphics& rGraphics);

    // Render path preview as a line through tile centers
    void RenderPathPreview_(Graphics& rGraphics);
};

} // namespace ac

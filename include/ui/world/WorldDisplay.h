#pragma once

#include "graphics/Graphics.h"
#include "game/units/Unit.h"
#include "ui/MapSurfaceRenderer.h"
#include "ui/UIElement.h"
#include "ui/world/MapViewport.h"
#include "ui/world/UnitMarkerRenderer.h"

#include <optional>
#include <unordered_set>

namespace ac
{

class GameState;
class WorldMap;
class Tile;
struct Path_t;

// Displays the world map as a grid of tiles.
// Each tile is painted by MapSurfaceRenderer (terrain, grid, objects, then bases). Bases,
// Sensors, Monoliths, and units are read live from GameState / WorldMap — no per-frame DTO
// rebuild.
class WorldDisplay
{
public:
    WorldDisplay(const GameState& rGameState, WindowLayout_t layout);

    // Set the unit currently selected by the player (highlighted on the map). Also used as
    // the path-preview line origin when a path is active.
    void SetSelectedUnit(const Unit* pUnit);

    // Forwarded to the unit markers. Null restores ordinary visibility.
    void SetPlaybackVisibleUnits(const std::unordered_set<UnitId_t>* pUnitIds);

    // Set the path preview to render (nullptr to clear). Pointer must remain valid until
    // the next Render call.
    void SetPathPreview(const Path_t* pPath);

    MapViewport& GetViewport() { return m_viewport; }
    const MapViewport& GetViewport() const { return m_viewport; }

    float GetEffectiveTileSize() const;
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
    MapSurfaceRenderer m_mapSurface;
    MapViewport m_viewport;

    // Faction base sprites (when extracted) plus name labels in faction colours.
    void RenderBases_(Graphics& rGraphics);

    // Render path preview as a line through tile centers
    void RenderPathPreview_(Graphics& rGraphics);
};

} // namespace ac

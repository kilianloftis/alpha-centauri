#pragma once

#include "graphics/Graphics.h"
#include "game/units/Unit.h"
#include "ui/MapRenderer.h"
#include "ui/UIElement.h"
#include "ui/world/MapViewport.h"

#include <optional>
#include <unordered_set>

namespace ac
{

class GameState;
class WorldMap;
class Tile;
struct Path_t;

// Displays the world map as a grid of tiles.
// MapRenderer paints the viewport's tiles as the player sees them: fog over remembered tiles,
// every base, and the units the player can see. Bases, Sensors, Monoliths, and units are read
// live from GameState / WorldMap — no per-frame DTO rebuild.
class WorldDisplay
{
public:
    WorldDisplay(const GameState& rGameState, MapRenderer& rMapRenderer, WindowLayout_t layout);

    // Set the unit currently selected by the player (highlighted on the map). Also used as
    // the path-preview line origin when a path is active.
    void SetSelectedUnit(const Unit* pUnit);

    // Units drawn even when IsUnitVisibleTo is false. Bombard playback points this at the
    // shrouded or concealed units on the target tile, then clears it with null.
    void SetPlaybackVisibleUnits(const std::unordered_set<UnitId_t>* pUnitIds);

    // Set the path preview to render (nullptr to clear). Pointer must remain valid until
    // the next Render call.
    void SetPathPreview(const Path_t* pPath);

    MapViewport& GetViewport() { return m_viewport; }
    const MapViewport& GetViewport() const { return m_viewport; }

    float GetEffectiveTileSize() const;
    int GetVisibleRows() const;

    // Where the last Render drew the unit's marker, if it drew one (combat hit overlays).
    std::optional<Rectangle_t> MarkerRectOf(UnitId_t unitId) const;

    // Render the world map using the stored layout.
    void Render(Graphics& rGraphics);

private:
    const GameState& m_rGameState;
    MapRenderer& m_rMapRenderer;
    const Unit* m_pSelectedUnit = nullptr;
    const Path_t* m_pPathPreview = nullptr;
    const std::unordered_set<UnitId_t>* m_pPlaybackVisibleUnits = nullptr;
    UnitMarkerRects_t m_unitMarkers;
    MapViewport m_viewport;

    // Render path preview as a line through tile centers
    void RenderPathPreview_(Graphics& rGraphics);
};

} // namespace ac

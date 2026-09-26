#pragma once

#include "game/units/Unit.h"
#include "graphics/Graphics.h"
#include "ui/UIElement.h"

#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace ac
{

class GameState;
class MapViewport;

// Draws unit chips on the map and caches each drawn marker's screen rect for the frame.
// Combat hit overlays (and later hit animations) look up that cache instead of re-finding
// units.
class UnitMarkerRenderer
{
public:
    void SetSelectedUnit(const Unit* pUnit) { m_pSelectedUnit = pUnit; }

    // Units drawn this frame even when IsUnitVisibleTo is false. Bombard playback points
    // this at the shrouded or concealed units on the target tile, then clears it.
    void SetPlaybackVisibleUnits(const std::unordered_set<UnitId_t>* pUnitIds)
    {
        m_pPlaybackVisibleUnits = pUnitIds;
    }

    // Clears the previous frame's cache, draws visible units via rViewport, and records
    // each marker rect keyed by unit id.
    void Render(Graphics& rGraphics, const GameState& rGameState, const MapViewport& rViewport);

    // Marker rect from the most recent Render, if that unit was drawn.
    std::optional<Rectangle_t> GetCachedMarkerRect(UnitId_t unitId) const;

    // First-slot marker rect on a tile (used when the unit was destroyed and never drawn).
    static Rectangle_t MarkerRectOnTile(float tileX, float tileY, float tileSize);

    // Draw a single unit chip into rMarker (map markers and dashboard stack share this).
    static void DrawMarker(Graphics& rGraphics, const Unit& rUnit, const Rectangle_t& rMarker,
                           bool bSelected);

    // Placeholder hit graphic over a marker. Replace with a hit animation later.
    static void DrawHitOverlay(Graphics& rGraphics, const Rectangle_t& rMarker);

private:
    const Unit* m_pSelectedUnit = nullptr;
    const std::unordered_set<UnitId_t>* m_pPlaybackVisibleUnits = nullptr;
    std::unordered_map<UnitId_t, Rectangle_t> m_markerRects;
};

} // namespace ac
